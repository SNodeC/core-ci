#include "Config.h"

#include <charconv>
#include <fstream>
#include <iostream>
#include <iterator>

namespace coreci::app {
    Config::Config(utils::SubCommand& section) {
        const std::map<std::string, std::string> defaults{{"registry", "SNodeC/Packages:ci/registry.json"},
                                                          {"target-catalog", "ci/targets.json"},
                                                          {"workflow-repository", "SNodeC/Packages"},
                                                          {"github-app-id", ""},
                                                          {"github-installation-id", ""},
                                                          {"github-app-key-file", ""},
                                                          {"callback-base-url", ""},
                                                          {"github-api-url", "https://api.github.com"},
                                                          {"github-ca-cert", ""},
                                                          {"db-host", "localhost"},
                                                          {"db-port", "0"},
                                                          {"db-socket", ""},
                                                          {"db-user", ""},
                                                          {"db-name", ""},
                                                          {"github_slots", "20"},
                                                          {"batch-delay", "120"},
                                                          {"queue-deadline", "7200"},
                                                          {"execution-deadline", "21600"},
                                                          {"callback-grace", "900"},
                                                          {"run-poll", "300"},
                                                          {"qualification_fault", ""},
                                                          {"dispatch-paused", ""},
                                                          {"webhook-secret-file", ""},
                                                          {"callback-secret-file", ""},
                                                          {"db-password-file", ""},
                                                          {"feed-branch", "feed"},
                                                          {"reconcile-interval", "900"},
                                                          {"idp-base-url", "https://core.snodec.org:8088"},
                                                          {"idp-client-id", ""},
                                                          {"idp-public-key-path", ""},
                                                          {"dashboard-users", ""}};
        for (const auto& [name, value] : defaults) {
            options[name] = section.addOption("--" + name, name, "value", value, CLI::Validator());
        }
    }
    std::string Config::get(const std::string& name) const {
        return options.at(name)->as<std::string>();
    }
    void Config::invalid(const std::string& name, bool& valid) {
        valid = false;
        std::cerr << "invalid option: " << name << '\n';
    }
    std::string Config::secret(const std::string& name, bool& valid) const {
        std::string result;
        if (!get(name).empty()) {
            std::ifstream input(get(name), std::ios::binary);
            if (!input) {
                invalid(name, valid);
            } else {
                result.assign(std::istreambuf_iterator<char>(input), {});
                if (!result.empty() && result.back() == '\n') {
                    result.pop_back();
                }
                if (result.find('\0') != std::string::npos) {
                    invalid(name, valid);
                }
            }
        }
        return result;
    }
    std::int64_t Config::number(const std::string& name, std::int64_t low, std::int64_t high, bool& valid) const {
        const auto value = get(name);
        std::int64_t result = 0;
        const auto parsed = std::from_chars(value.data(), value.data() + value.size(), result);
        if (parsed.ec != std::errc{} || parsed.ptr != value.data() + value.size() || result < low || result > high) {
            invalid(name, valid);
        }
        return result;
    }
} // namespace coreci::app
