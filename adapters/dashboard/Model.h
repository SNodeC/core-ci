#pragma once
#include "adapters/github/Declarations.h"
#include "app/StatusJson.h"
namespace coreci::dashboard {
    std::string model(const WaveController& engine,
                      const github::LoadedDeclarations& declarations,
                      std::uint64_t generation,
                      bool ready,
                      const std::string& email = {},
                      const std::string& loadError = {});
    std::variant<Unit, github::Error> unit(const std::string& body);
    std::string result(const std::string& text, bool error);
} // namespace coreci::dashboard
