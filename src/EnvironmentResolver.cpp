#include "EnvironmentResolver.h"

#include "Definition.h"
#include "HttpsUrl.h"
#include "Version.h"

#include <algorithm>
#include <array>
#include <openssl/evp.h>
#include <optional>
#include <regex>
#include <sstream>
#include <stdexcept>
#include <web/http/http_utils.h>

namespace coreci {
    namespace {
        constexpr auto accept =
            "application/vnd.oci.image.index.v1+json, application/vnd.oci.image.manifest.v1+json, "
            "application/vnd.docker.distribution.manifest.list.v2+json, application/vnd.docker.distribution.manifest.v2+json";
        constexpr std::size_t maximumDocumentBytes = 2 * 1024 * 1024;

        void require(bool condition, const std::string& message) {
            if (!condition) {
                throw std::invalid_argument(message);
            }
        }

        bool digestSyntax(const std::string& value) {
            static const std::regex syntax(R"(^sha256:[a-f0-9]{64}$)");
            return std::regex_match(value, syntax);
        }

        void descriptorValid(const nlohmann::json& descriptor) {
            require(digestSyntax(descriptor.at("digest").get<std::string>()) && descriptor.at("size").is_number_unsigned() &&
                        descriptor.at("size").get<std::size_t>() <= maximumDocumentBytes,
                    "registry descriptor requires a bounded SHA-256 document");
        }
    } // namespace

    RegistryConfig::RegistryConfig(utils::SubCommand* parent)
        : SubCommand(parent, this, "Build environment sources") {
        setConfigurable(addOptionVariable("--default-registry",
                                          defaultRegistry,
                                          "HTTPS registry authority for unqualified image names",
                                          "host[:port]",
                                          CLI::TypeValidator<std::string>()),
                        true);
        setConfigurable(addOptionVariable("--default-namespace",
                                          defaultNamespace,
                                          "Namespace for single-component image names",
                                          "namespace",
                                          CLI::TypeValidator<std::string>()),
                        true);
        setConfigurable(addOptionVariable("--allowed-origin",
                                          allowedOrigins,
                                          "Approved HTTPS registry, token or document-redirect origin",
                                          "origin",
                                          CLI::TypeValidator<std::string>())
                            ->take_all(),
                        true);
        setConfigurable(
            addOptionVariable(
                "--sdk-base-url", sdkBaseUrl, "OpenWrt HTTPS release metadata service origin", "url", CLI::TypeValidator<std::string>()),
            true);
    }

    EnvironmentResolver::EnvironmentResolver(const RegistryConfig& config)
        : config(config)
        , http("environment-http") {
    }

    void EnvironmentResolver::configure() {
        http.configure();
        for (const auto& value : config.allowedOrigins) {
            const HttpsUrl url(value);
            require(url.path == "/", "registry origin configuration cannot contain a path or query");
            allowedOrigins.insert(url.origin);
        }
        if (!config.sdkBaseUrl.empty()) {
            require(HttpsUrl(config.sdkBaseUrl).path == "/", "SDK metadata service must be an HTTPS origin");
        }
        if (!config.defaultRegistry.empty()) {
            const HttpsUrl url("https://" + config.defaultRegistry);
            require(url.path == "/" && allowedOrigins.contains(url.origin), "default image registry must be an approved origin");
        }
        if (!config.defaultNamespace.empty()) {
            static const std::regex name(R"(^[a-z0-9]+(?:[._-][a-z0-9]+)*$)");
            require(std::regex_match(config.defaultNamespace, name), "invalid default registry namespace");
        }
    }

    void EnvironmentResolver::resolve(const nlohmann::json& source, const Callback& callback) {
        try {
            Definition::parse(source.at("definition"));
            auto reading = std::make_shared<Reading>();
            reading->source = source;
            reading->source["environments"] = nlohmann::json::object();
            reading->callback = callback;
            next(reading);
        } catch (const std::exception& error) {
            callback({nullptr, error.what()});
        }
    }

    void EnvironmentResolver::next(const std::shared_ptr<Reading>& reading) {
        try {
            const auto& rows = reading->source.at("definition").at("matrix");
            if (reading->row == rows.size()) {
                reading->callback({reading->source, ""});
            } else {
                const auto& row = rows.at(reading->row);
                const auto& build = row.at("build");
                if (row.at("family") == "openwrt") {
                    sdk(reading);
                } else {
                    std::string image = build.at("image").get<std::string>();
                    const auto slash = image.find('/');
                    const auto first = image.substr(0, slash);
                    if (slash != std::string::npos && (first.find_first_of(".:") != std::string::npos || first == "localhost")) {
                        reading->origin = HttpsUrl("https://" + first).origin;
                        image.erase(0, slash + 1);
                    } else {
                        require(!config.defaultRegistry.empty(), "unqualified image requires registry.default-registry");
                        reading->origin = HttpsUrl("https://" + config.defaultRegistry).origin;
                    }
                    require(allowedOrigins.contains(reading->origin), "image registry origin is not approved");
                    const auto at = image.find('@');
                    const auto colon = image.rfind(':');
                    if (at != std::string::npos) {
                        reading->reference = image.substr(at + 1);
                        image.resize(at);
                        require(digestSyntax(reading->reference), "image digest must be SHA-256");
                        const auto tag = image.rfind(':');
                        if (tag != std::string::npos) {
                            image.resize(tag);
                        }
                    } else {
                        require(colon != std::string::npos && colon > image.rfind('/') + 1,
                                "native image must have an explicit tag or digest");
                        reading->reference = image.substr(colon + 1);
                        image.resize(colon);
                        static const std::regex tag(R"(^[A-Za-z0-9_][A-Za-z0-9_.-]{0,127}$)");
                        require(std::regex_match(reading->reference, tag), "unsupported image tag");
                    }
                    if (image.find('/') == std::string::npos && !config.defaultNamespace.empty()) {
                        image = config.defaultNamespace + "/" + image;
                    }
                    static const std::regex repository(
                        R"(^[a-z0-9]+(?:(?:[._]|__|-+)[a-z0-9]+)*(?:/[a-z0-9]+(?:(?:[._]|__|-+)[a-z0-9]+)*)*$)");
                    require(image.size() <= 255 && std::regex_match(image, repository), "unsupported registry repository name");
                    reading->repository = image;
                    reading->token.clear();
                    reading->rootDigest.clear();
                    const auto platform = build.at("platform").get<std::string>();
                    static const std::regex platformSyntax(R"(^linux/([a-z0-9_]+)(?:/([A-Za-z0-9_.-]+))?$)");
                    std::smatch match;
                    require(std::regex_match(platform, match, platformSyntax), "native platform must be linux/architecture[/variant]");
                    reading->os = "linux";
                    reading->arch = match[1].str();
                    reading->variant = match[2].str();
                    manifest(reading, nullptr, 0);
                }
            }
        } catch (const std::exception& error) {
            reading->callback({nullptr, error.what()});
        }
    }

    std::string EnvironmentResolver::verify(const HttpResult& result, const nlohmann::json& descriptor) {
        require(result.error.empty() && result.status == 200 && result.body.size() <= maximumDocumentBytes,
                "registry document request failed (HTTP " + std::to_string(result.status) + "): " + result.error);
        std::array<unsigned char, EVP_MAX_MD_SIZE> bytes{};
        unsigned int size = 0;
        require(EVP_Digest(result.body.data(), result.body.size(), bytes.data(), &size, EVP_sha256(), nullptr) == 1,
                "image digest calculation failed");
        std::string digest = "sha256:";
        constexpr char hex[] = "0123456789abcdef";
        for (unsigned int index = 0; index < size; ++index) {
            digest.push_back(hex[bytes[index] >> 4]);
            digest.push_back(hex[bytes[index] & 15]);
        }
        if (!descriptor.is_null()) {
            require(descriptor.at("digest") == digest && descriptor.at("size") == result.body.size(),
                    "registry document differs from its digest/size descriptor");
        }
        const auto header = result.headers.find("Docker-Content-Digest");
        require(header == result.headers.end() || header->second == digest, "registry digest header differs from document bytes");
        return digest;
    }

    void EnvironmentResolver::sdk(const std::shared_ptr<Reading>& reading) {
        require(!config.sdkBaseUrl.empty(), "configure registry.sdk-base-url for latest OpenWrt SDK resolution");
        const HttpsUrl base(config.sdkBaseUrl);
        const auto row = reading->source.at("definition").at("matrix").at(reading->row);
        http.request(base.host, base.port, "GET", "/.versions.json", {}, "", [this, reading, base, row](const HttpResult& versions) {
            try {
                require(versions.status == 200 && versions.error.empty(), "OpenWrt release metadata is unavailable");
                const auto document = nlohmann::json::parse(versions.body);
                std::optional<Version> highest;
                const auto prefix = row.at("suite").get<std::string>() + ".";
                require(document.at("versions_list").is_array() && document.at("versions_list").size() <= 4096,
                        "invalid OpenWrt release list");
                for (const auto& item : document.at("versions_list")) {
                    const auto text = item.get<std::string>();
                    if (text.starts_with(prefix)) {
                        const Version candidate(text);
                        if (candidate.prerelease.empty() && (!highest || candidate.compare(*highest) > 0)) {
                            highest = candidate;
                        }
                    }
                }
                require(highest.has_value(), "no stable SDK release exists for the configured OpenWrt series");
                const auto release = highest->tag;
                const auto root = "/releases/" + release + "/targets/" + row.at("build").at("target").get<std::string>() + "/";
                http.request(
                    base.host,
                    base.port,
                    "GET",
                    root + "profiles.json",
                    {},
                    "",
                    [this, reading, base, row, root, release](const HttpResult& profiles) {
                        try {
                            require(profiles.status == 200 && profiles.error.empty(), "selected OpenWrt target metadata is unavailable");
                            const auto profile = nlohmann::json::parse(profiles.body);
                            require(profile.at("version_number") == release && profile.at("target") == row.at("build").at("target") &&
                                        profile.at("arch_packages") == row.at("arch"),
                                    "OpenWrt release/target/package architecture mismatch");
                            const auto packages = profile.at("default_packages").get<std::vector<std::string>>();
                            const bool ipk = std::find(packages.begin(), packages.end(), "opkg") != packages.end();
                            const bool apk = std::any_of(packages.begin(), packages.end(), [](const auto& name) {
                                return name == "apk" || name == "apk-mbedtls" || name == "apk-openssl";
                            });
                            require((row.at("build").at("format") == "ipk" && ipk && !apk) ||
                                        (row.at("build").at("format") == "apk" && apk && !ipk),
                                    "configured package format differs from the latest OpenWrt target");
                            http.request(
                                base.host,
                                base.port,
                                "GET",
                                root + "sha256sums",
                                {},
                                "",
                                [this, reading, base, row, root, release](const HttpResult& sums) {
                                    try {
                                        require(sums.status == 200 && sums.error.empty(), "latest SDK checksum metadata is unavailable");
                                        static const std::regex entry(
                                            R"(^([a-f0-9]{64}) \*?(openwrt-sdk-[A-Za-z0-9_.-]+\.tar\.(xz|zst|gz))$)");
                                        std::istringstream lines(sums.body);
                                        std::string line;
                                        nlohmann::json selected;
                                        while (std::getline(lines, line)) {
                                            std::smatch match;
                                            if (std::regex_match(line, match, entry)) {
                                                require(selected.is_null(),
                                                        "latest SDK archive is ambiguous; unsupported worker configuration");
                                                selected = {{"url", base.origin + root + match[2].str()}, {"sha256", match[1].str()}};
                                            }
                                        }
                                        require(!selected.is_null(), "latest SDK archive is absent from official checksum metadata");
                                        reading->source["environments"][row.at("id").get<std::string>()] = {{"release", release},
                                                                                                            {"sdk", selected}};
                                        ++reading->row;
                                        next(reading);
                                    } catch (const std::exception& error) {
                                        reading->callback({nullptr, error.what()});
                                    }
                                });
                        } catch (const std::exception& error) {
                            reading->callback({nullptr, error.what()});
                        }
                    });
            } catch (const std::exception& error) {
                reading->callback({nullptr, error.what()});
            }
        });
    }

    void EnvironmentResolver::manifest(const std::shared_ptr<Reading>& reading, const nlohmann::json& descriptor, unsigned int depth) {
        const auto reference = descriptor.is_null() ? reading->reference : descriptor.at("digest").get<std::string>();
        fetch(reading,
              reading->origin + "/v2/" + reading->repository + "/manifests/" + reference,
              [this, reading, descriptor, depth](const HttpResult& result) {
                  try {
                      const auto digest = verify(result, descriptor);
                      if (depth == 0) {
                          require(!digestSyntax(reading->reference) || reading->reference == digest, "requested image digest changed");
                          reading->rootDigest = digest;
                      }
                      const auto document = nlohmann::json::parse(result.body);
                      require(document.at("schemaVersion") == 2, "unsupported image schema version");
                      require(descriptor.is_null() || descriptor.at("mediaType") == document.at("mediaType"),
                              "image document media type differs from its descriptor");
                      const auto media = document.at("mediaType").get<std::string>();
                      if (media == "application/vnd.oci.image.index.v1+json" ||
                          media == "application/vnd.docker.distribution.manifest.list.v2+json") {
                          require(depth < 4 && document.at("manifests").is_array() && document.at("manifests").size() <= 1024,
                                  "image index exceeds supported bounds");
                          nlohmann::json selected;
                          for (const auto& entry : document.at("manifests")) {
                              if (entry.contains("platform")) {
                                  const auto& platform = entry.at("platform");
                                  if (platform.value("os", "") == reading->os && platform.value("architecture", "") == reading->arch &&
                                      (reading->variant.empty() || platform.value("variant", "") == reading->variant)) {
                                      require(selected.is_null(),
                                              "image platform matches more than one manifest; select an explicit variant");
                                      selected = entry;
                                  }
                              }
                          }
                          require(!selected.is_null(), "image has no manifest for the exact configured platform");
                          descriptorValid(selected);
                          manifest(reading, selected, depth + 1);
                      } else {
                          require(media == "application/vnd.oci.image.manifest.v1+json" ||
                                      media == "application/vnd.docker.distribution.manifest.v2+json",
                                  "unsupported image manifest media type");
                          const auto& configDescriptor = document.at("config");
                          descriptorValid(configDescriptor);
                          require(configDescriptor.at("mediaType") == "application/vnd.oci.image.config.v1+json" ||
                                      configDescriptor.at("mediaType") == "application/vnd.docker.container.image.v1+json",
                                  "unsupported image configuration media type");
                          reading->manifestDigest = digest;
                          configuration(reading, configDescriptor);
                      }
                  } catch (const std::exception& error) {
                      reading->callback({nullptr, error.what()});
                  }
              });
    }

    void EnvironmentResolver::configuration(const std::shared_ptr<Reading>& reading, const nlohmann::json& descriptor) {
        fetch(reading,
              reading->origin + "/v2/" + reading->repository + "/blobs/" + descriptor.at("digest").get<std::string>(),
              [this, reading, descriptor](const HttpResult& result) {
                  try {
                      verify(result, descriptor);
                      const auto configuration = nlohmann::json::parse(result.body);
                      require(configuration.at("os") == reading->os && configuration.at("architecture") == reading->arch &&
                                  (reading->variant.empty() || configuration.value("variant", "") == reading->variant),
                              "image configuration disagrees with the requested platform");
                      const auto& row = reading->source.at("definition").at("matrix").at(reading->row);
                      reading->source["environments"][row.at("id").get<std::string>()] = {
                          {"image", row.at("build").at("image")},
                          {"platform", row.at("build").at("platform")},
                          {"digest", reading->manifestDigest},
                          {"index_digest", reading->rootDigest},
                          {"config_digest", descriptor.at("digest")},
                          {"resolved_image", reading->origin.substr(8) + "/" + reading->repository + "@" + reading->manifestDigest}};
                      ++reading->row;
                      next(reading);
                  } catch (const std::exception& error) {
                      reading->callback({nullptr, error.what()});
                  }
              });
    }

    void EnvironmentResolver::fetch(const std::shared_ptr<Reading>& reading,
                                    const std::string& value,
                                    const HttpClient::Callback& callback,
                                    bool authenticate,
                                    unsigned int redirects) {
        try {
            const HttpsUrl url(value);
            require(allowedOrigins.contains(url.origin), "registry/token/redirect origin is not approved");
            web::http::CiStringMap<std::string> headers{{"Accept", accept}};
            if (url.origin == reading->origin && !reading->token.empty()) {
                headers["Authorization"] = "Bearer " + reading->token;
            }
            http.request(url.host,
                         url.port,
                         "GET",
                         url.path,
                         headers,
                         "",
                         [this, reading, value, callback, authenticate, redirects, url](const HttpResult& result) {
                             if (result.error.empty() && result.status == 401 && authenticate && url.origin == reading->origin) {
                                 authorize(reading, result, [this, reading, value, callback, redirects]() {
                                     fetch(reading, value, callback, false, redirects);
                                 });
                             } else if (result.error.empty() && (result.status == 302 || result.status == 307)) {
                                 const auto location = result.headers.find("Location");
                                 if (redirects < 3 && location != result.headers.end()) {
                                     fetch(reading, location->second, callback, false, redirects + 1);
                                 } else {
                                     callback({0, {}, "", "registry redirect limit exceeded or Location is missing"});
                                 }
                             } else {
                                 callback(result);
                             }
                         });
        } catch (const std::exception& error) {
            callback({0, {}, "", error.what()});
        }
    }

    void EnvironmentResolver::authorize(const std::shared_ptr<Reading>& reading,
                                        const HttpResult& challenge,
                                        const std::function<void()>& next) {
        try {
            const auto header = challenge.headers.find("WWW-Authenticate");
            require(header != challenge.headers.end() && header->second.starts_with("Bearer "),
                    "registry requires unsupported authentication; configure an anonymous pull endpoint");
            std::string parameters = header->second.substr(7);
            static const std::regex field(R"regex(^\s*(realm|service|scope)="([^"\\\r\n]*)"\s*(?:,|$))regex");
            std::map<std::string, std::string> values;
            while (!parameters.empty()) {
                std::smatch match;
                require(std::regex_search(parameters, match, field) && values.emplace(match[1].str(), match[2].str()).second,
                        "unsupported or duplicated registry authentication parameter");
                parameters.erase(0, match.length());
            }
            const auto scope = "repository:" + reading->repository + ":pull";
            require(!values.contains("scope") || values.at("scope") == scope, "registry requested a different authentication scope");
            const HttpsUrl realm(values.at("realm"));
            require(allowedOrigins.contains(realm.origin), "registry token service origin is not approved");
            const auto path = realm.path + (realm.path.find('?') == std::string::npos ? "?" : "&") +
                              "scope=" + httputils::url_encode(scope) +
                              (values.contains("service") ? "&service=" + httputils::url_encode(values.at("service")) : "");
            http.request(
                realm.host, realm.port, "GET", path, {{"Accept", "application/json"}}, "", [reading, next](const HttpResult& result) {
                    try {
                        require(result.status == 200 && result.error.empty(),
                                "registry token acquisition failed (HTTP " + std::to_string(result.status) + "): " + result.error);
                        const auto document = nlohmann::json::parse(result.body);
                        const auto token = document.value("token", document.value("access_token", std::string{}));
                        require(!token.empty() && token.size() <= 16384 &&
                                    std::all_of(token.begin(),
                                                token.end(),
                                                [](unsigned char c) {
                                                    return c >= 33 && c <= 126;
                                                }),
                                "registry returned an invalid pull token");
                        reading->token = token;
                        next();
                    } catch (const std::exception& error) {
                        reading->callback({nullptr, error.what()});
                    }
                });
        } catch (const std::exception& error) {
            reading->callback({nullptr, error.what()});
        }
    }
} // namespace coreci
