#pragma once
#include <map>
#include <utils/SubCommand.h>

namespace coreci::app {
    class Config {
    public:
        explicit Config(utils::SubCommand& section);
        std::string get(const std::string& name) const;
        std::string secret(const std::string& name, bool& valid) const;
        std::int64_t number(const std::string& name, std::int64_t low, std::int64_t high, bool& valid) const;
        static void invalid(const std::string& name, bool& valid);

    private:
        std::map<std::string, CLI::Option*> options;
    };
} // namespace coreci::app
