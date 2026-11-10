#pragma once
#include "app/JobCache.h"
#include "app/StatusJson.h"
namespace coreci::dashboard {
    std::vector<github::JobRequest> jobRequests(const Engine& engine);
    std::string model(const Engine& engine,
                      const github::LoadedDeclarations& declarations,
                      const app::JobCache& jobs,
                      std::uint64_t generation,
                      bool ready,
                      const std::string& email = {});
    std::variant<Unit, github::Error> retry(const std::string& body);
    std::string result(const std::string& text, bool error);
} // namespace coreci::dashboard
