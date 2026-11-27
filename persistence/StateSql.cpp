#include "StateSql.h"

#include <charconv>
#include <stdexcept>

namespace coreci::persistence {
    namespace {
        std::string unhex(const std::string& text) {
            std::string result;
            if (text.size() % 2 != 0) {
                throw std::runtime_error("invalid hexadecimal data in state");
            }
            for (std::size_t offset = 0; offset < text.size(); offset += 2) {
                unsigned char value = 0;
                const auto end = text.data() + offset + 2;
                const auto parsed = std::from_chars(text.data() + offset, end, value, 16);
                if (parsed.ec != std::errc{} || parsed.ptr != end) {
                    throw std::runtime_error("invalid hexadecimal data in state");
                }
                result += static_cast<char>(value);
            }
            return result;
        }
    } // namespace

    std::string sqlText(const std::string& bytes) {
        static constexpr char digits[] = "0123456789ABCDEF";
        std::string result = "_utf8mb4 X'";
        for (const unsigned char byte : bytes) {
            result += digits[byte >> 4];
            result += digits[byte & 15];
        }
        result += "'";
        return result;
    }

    void readRecord(Json& state, const std::string& section, const std::string& key, const std::string& data) {
        const auto name = unhex(section);
        const auto item = unhex(key);
        const auto record = Json::parse(unhex(data));
        if (!state.contains(name) || item.find('\0') != std::string::npos) {
            throw std::runtime_error("unknown state record");
        }
        if (state.at(name).is_object()) {
            if (item.empty()) {
                throw std::runtime_error("missing record key");
            }
            state[name][item] = record;
        } else if (item.empty() &&
                   (state.at(name).type() == record.type() || (state.at(name).is_number_integer() && record.is_number_integer()))) {
            state[name] = record;
        } else {
            throw std::runtime_error("unexpected state record type");
        }
    }

    std::vector<std::string> stateSql(const Json& before, const Json& after, const std::string& description) {
        std::vector<std::string> statements;
        const auto write = [&statements](const auto& section, const auto& key, const Json& value) {
            statements.push_back("INSERT INTO coreci_records (section,record_key,payload) VALUES (" + sqlText(section) + "," +
                                 sqlText(key) + "," + sqlText(value.dump()) + ") ON DUPLICATE KEY UPDATE payload=VALUES(payload)");
        };
        for (const auto& [section, entries] : after.items()) {
            const auto& prior = before.at(section);
            if (entries.is_object()) {
                for (const auto& [key, entry] : entries.items()) {
                    if (!prior.contains(key) || prior.at(key) != entry) {
                        write(section, key, entry);
                    }
                }
                for (const auto& [key, _] : prior.items()) {
                    if (!entries.contains(key)) {
                        statements.push_back("DELETE FROM coreci_records WHERE section=" + sqlText(section) +
                                             " AND record_key=" + sqlText(key));
                    }
                }
            } else if (prior != entries) {
                write(section, "", entries);
            }
        }
        if (!description.empty() && !statements.empty()) {
            statements.push_back("INSERT INTO coreci_log (description) VALUES (" + sqlText(description) + ")");
        }
        return statements;
    }
} // namespace coreci::persistence
