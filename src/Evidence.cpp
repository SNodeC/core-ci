#include "Auth.h"
#include "Controller.h"

#include <algorithm>
#include <map>
#include <regex>
#include <set>
#include <zlib.h>

namespace ci {

    namespace {

        void relativePath(const std::string& name) {
            const std::string bounded = "/" + name + "/";
            require(!name.empty() && name.size() <= 512 && name.front() != '/' && name.find('\\') == std::string::npos &&
                        bounded.find("//") == std::string::npos && bounded.find("/./") == std::string::npos &&
                        bounded.find("/../") == std::string::npos &&
                        std::all_of(name.begin(),
                                    name.end(),
                                    [](unsigned char c) {
                                        return c > 32 && c < 127;
                                    }),
                    "Unsafe artifact path");
        }

        std::uint64_t number(const std::string& bytes, std::size_t offset, std::size_t width) {
            require(offset <= bytes.size() && width <= bytes.size() - offset && width <= 8, "Truncated ZIP metadata");
            std::uint64_t result = 0;
            for (std::size_t index = 0; index < width; ++index) {
                result |= static_cast<std::uint64_t>(static_cast<unsigned char>(bytes[offset + index])) << (index * 8);
            }
            return result;
        }

        Json packageVersion(const Json& selected) {
            std::string tag = selected.at("source").at("tag");
            const std::string format = selected.at("target").at("build").at("format");
            if (tag.front() == 'v') {
                tag.erase(0, 1);
            }
            tag = tag.substr(0, tag.find('+'));
            const auto dash = tag.find('-');
            std::string version = tag.substr(0, dash);
            if (dash != std::string::npos) {
                std::vector<unsigned int> tokens;
                std::string pre = tag.substr(dash + 1) + ".";
                std::size_t start = 0;
                for (std::size_t index = 0; index < pre.size(); ++index) {
                    if (pre[index] == '.') {
                        const std::string identifier = pre.substr(start, index - start);
                        const bool numeric = identifier.find_first_not_of("0123456789") == std::string::npos;
                        tokens.push_back(numeric ? 1 : 2);
                        if (numeric) {
                            tokens.push_back(static_cast<unsigned int>(identifier.size()));
                        }
                        for (const unsigned char character : identifier) {
                            tokens.push_back(numeric ? character - '0' : character);
                        }
                        if (!numeric) {
                            tokens.push_back(0);
                        }
                        start = index + 1;
                    }
                }
                tokens.push_back(0);
                version += format == "apk" ? "_pre0" : "~";
                for (std::size_t index = 0; index < tokens.size(); ++index) {
                    version += (format == "apk" ? "_p" : (index == 0 ? "p" : ".p")) + std::to_string(tokens[index]);
                }
            }
            const std::string release = (format == "ipk" || format == "apk" ? "r" : "") + selected.at("revision").dump();
            require(version.size() + release.size() < 240, "Package version exceeds contract bound");
            return {{"version", version}, {"release", release}, {"full", version + "-" + release}};
        }

        Json compatibility(const Json& selected, const Json& environment) {
            Json result = coordinates(selected.at("target"));
            if (result.at("family") != "native") {
                result["release"] = environment.at("release");
                result["sdk_sha256"] = environment.at("sdk").at("sha256");
                result["toolchain"] = environment.at("toolchain");
            }
            return result;
        }

    } // namespace

    Json artifactManifest(const std::string& bytes, const Json& assignment) {
        const auto end = bytes.rfind(std::string("PK\5\6", 4));
        require(end != std::string::npos && number(bytes, end + 4, 4) == 0 && end + 22 + number(bytes, end + 20, 2) == bytes.size(),
                "Invalid ZIP end record");
        auto count = number(bytes, end + 10, 2);
        auto offset = number(bytes, end + 16, 4);
        if (count == 65535 || offset == UINT32_MAX) {
            require(end >= 20 && number(bytes, end - 20, 4) == 0x07064b50, "Missing ZIP64 locator");
            const auto extended = number(bytes, end - 12, 8);
            require(number(bytes, extended, 4) == 0x06064b50 && number(bytes, extended + 16, 8) == 0, "Invalid ZIP64 record");
            count = number(bytes, extended + 32, 8);
            offset = number(bytes, extended + 48, 8);
        }
        require(count > 0 && count <= 20000 && bytes.size() <= 512 * 1024 * 1024, "ZIP resource bound exceeded");
        std::map<std::string, Json> files;
        std::set<std::string> entries;
        std::string manifest;
        std::uint64_t expanded = 0;
        for (std::uint64_t index = 0; index < count; ++index) {
            require(number(bytes, offset, 4) == 0x02014b50 && (number(bytes, offset + 8, 2) & 1) == 0, "Unsupported ZIP entry");
            const auto method = number(bytes, offset + 10, 2);
            auto compressed = number(bytes, offset + 20, 4);
            auto size = number(bytes, offset + 24, 4);
            const auto length = number(bytes, offset + 28, 2);
            const auto extra = number(bytes, offset + 30, 2);
            auto local = number(bytes, offset + 42, 4);
            require(offset + 46 + length + extra <= bytes.size(), "Truncated ZIP entry");
            std::string name = bytes.substr(offset + 46, length);
            const bool directory = name.ends_with('/');
            if (directory) {
                name.pop_back();
            }
            relativePath(name);
            require(entries.insert(name).second, "Duplicate artifact entry");
            const auto mode = (number(bytes, offset + 38, 4) >> 16) & 0170000;
            require(mode == 0 || mode == 0100000 || mode == 0040000, "Artifact links and special files forbidden");
            for (auto field = offset + 46 + length; field + 4 <= offset + 46 + length + extra;) {
                const auto kind = number(bytes, field, 2);
                const auto width = number(bytes, field + 2, 2);
                require(field + 4 + width <= offset + 46 + length + extra, "Truncated ZIP extra field");
                if (kind == 1) {
                    auto value = field + 4;
                    for (auto* item : {&size, &compressed, &local}) {
                        if (*item == UINT32_MAX) {
                            require(value + 8 <= field + 4 + width, "Truncated ZIP64 entry");
                            *item = number(bytes, value, 8);
                            value += 8;
                        }
                    }
                }
                field += width + 4;
            }
            require(size <= 512 * 1024 * 1024 && (expanded += size) <= 1024 * 1024 * 1024, "Expanded artifact exceeds bound");
            require(number(bytes, local, 4) == 0x04034b50, "Missing local ZIP header");
            const auto data = local + 30 + number(bytes, local + 26, 2) + number(bytes, local + 28, 2);
            require(data <= bytes.size() && compressed <= bytes.size() - data, "Truncated compressed artifact");
            std::string output(static_cast<std::size_t>(size) + 1, '\0');
            if (method == 0) {
                require(size == compressed, "Stored ZIP size mismatch");
                output.assign(bytes, data, size);
            } else {
                require(method == 8, "Unsupported ZIP compression");
                z_stream stream{};
                stream.next_in = reinterpret_cast<Bytef*>(const_cast<char*>(bytes.data() + data));
                stream.avail_in = static_cast<uInt>(compressed);
                stream.next_out = reinterpret_cast<Bytef*>(output.data());
                stream.avail_out = static_cast<uInt>(output.size());
                require(inflateInit2(&stream, -MAX_WBITS) == Z_OK, "Cannot initialize ZIP decompressor");
                const int status = inflate(&stream, Z_FINISH);
                inflateEnd(&stream);
                require(status == Z_STREAM_END && stream.total_out == size && stream.total_in == compressed, "Invalid deflated artifact");
                output.resize(size);
            }
            require(crc32(0, reinterpret_cast<const Bytef*>(output.data()), static_cast<uInt>(output.size())) ==
                        number(bytes, offset + 16, 4),
                    "ZIP CRC mismatch");
            if (!directory) {
                files.emplace(name, Json{{"size", size}, {"sha256", hash(output)}});
            }
            if (name == "manifest.json") {
                require(output.size() <= 2 * 1024 * 1024, "Manifest exceeds bound");
                manifest = output;
            }
            offset += 46 + length + extra + number(bytes, offset + 32, 2);
        }
        Json result = Json::parse(manifest);
        for (const auto& key : {"schema", "execution", "attempt", "revision", "artifact_name", "source", "target", "dependencies"}) {
            require(result.at(key) == assignment.at(key), "Build manifest identity mismatch");
        }
        require(result.at("assignment_sha256") == hash(assignment.dump()) && !result.contains("build_job"), "Untrusted manifest authority");
        require(result.at("package_version") == packageVersion(assignment), "Build package version mismatch");
        if (result.contains("summary")) {
            const auto& summary = result.at("summary");
            require(summary.is_object() && summary.dump().size() <= 8192, "Invalid build summary");
            for (const auto& [key, value] : summary.items()) {
                require(key == "compilers" || key == "tests", "Unknown summary field");
            }
            const Json compilers = summary.value("compilers", Json::array());
            require(compilers.is_array() && compilers.size() <= 16, "Invalid compiler summary");
            for (const auto& compiler : compilers) {
                require(compiler.is_object() && compiler.size() == 2, "Invalid compiler fields");
                for (const auto& key : {"name", "version"}) {
                    const auto text = compiler.at(key).get<std::string>();
                    require(!text.empty() && text.size() <= 512 &&
                                std::all_of(text.begin(),
                                            text.end(),
                                            [](unsigned char c) {
                                                return c >= 32;
                                            }),
                            "Invalid compiler text");
                }
            }
            if (summary.contains("tests")) {
                const auto& tests = summary.at("tests");
                require(tests.is_object(), "Invalid test summary");
                for (const auto& [key, value] : tests.items()) {
                    require(key == "passed" || key == "failed" || key == "skipped" || key == "coverage_percent" || key == "report_url",
                            "Unknown test field");
                }
                for (const auto& key : {"passed", "failed", "skipped"}) {
                    require(tests.at(key).is_number_integer() && tests.at(key) >= 0 && tests.at(key) <= 10000000, "Invalid test count");
                }
                if (tests.contains("coverage_percent")) {
                    require(tests.at("coverage_percent").is_number() && tests.at("coverage_percent") >= 0 &&
                                tests.at("coverage_percent") <= 100,
                            "Invalid coverage");
                }
                if (tests.contains("report_url")) {
                    const auto url = tests.at("report_url").get<std::string>();
                    const auto host = url.substr(0, url.find_first_of("/?#", 8));
                    require(url.size() <= 2048 && url.starts_with("https://") && host.size() > 8 && host.find('@') == std::string::npos &&
                                std::all_of(url.begin(),
                                            url.end(),
                                            [](unsigned char c) {
                                                return c >= 32;
                                            }),
                            "Invalid test report URL");
                }
            }
        }
        const auto& target = assignment.at("target");
        const auto& environment = result.at("environment");
        if (target.at("family") == "native") {
            require(environment.at("image") == target.at("build").at("image") &&
                        environment.at("platform") == target.at("build").at("platform") &&
                        std::regex_match(environment.at("digest").get<std::string>(), std::regex("sha256:[a-f0-9]{64}")) &&
                        std::regex_match(environment.at("config_digest").get<std::string>(), std::regex("sha256:[a-f0-9]{64}")) &&
                        environment.at("resolved_image").get<std::string>().ends_with("@" + environment.at("digest").get<std::string>()),
                    "Build environment differs from selection");
        } else {
            require(environment.at("release").get<std::string>().starts_with(target.at("suite").get<std::string>() + ".") &&
                        std::regex_match(environment.at("sdk").at("sha256").get<std::string>(), std::regex("[a-f0-9]{64}")),
                    "Invalid resolved SDK identity");
        }
        require(result.at("packages").is_array() && !result.at("packages").empty() && result.at("packages").size() <= 128,
                "Invalid package outputs");
        std::size_t expected = 1;
        for (const auto& package : result.at("packages")) {
            const std::string name = package.at("file");
            const std::string component = package.at("component");
            const std::string space = assignment.at("source").at("package");
            require((component == space || component.starts_with(space + "-")) &&
                        std::regex_match(component, std::regex("[a-z0-9][a-z0-9+.-]{0,127}")),
                    "Package outside assigned namespace");
            require(files.contains(name) && package.at("sha256") == files.at(name).at("sha256") &&
                        package.at("size") == files.at(name).at("size") && package.at("component") == package.at("name") &&
                        package.at("arch") == assignment.at("target").at("arch") &&
                        package.at("version") == result.at("package_version").at("full"),
                    "Invalid package artifact identity");
            ++expected;
        }
        if (!result.at("development_export").is_null()) {
            require(result.at("development_export").at("file") == "development.tar.gz" &&
                        result.at("development_export").at("compatibility") == compatibility(assignment, result.at("environment")) &&
                        result.at("development_export").at("sha256") == files.at("development.tar.gz").at("sha256") &&
                        result.at("development_export").at("size") == files.at("development.tar.gz").at("size"),
                    "Invalid development export");
            ++expected;
        }
        require(files.size() == expected, "Undeclared artifact files");
        return result;
    }

} // namespace ci
