#pragma once
#include "HttpClient.h"
#include "core/Scheduler.h"

namespace coreci::github {
    struct DeclarationFile {
        std::string owner, repo, path;
    };
    std::variant<DeclarationFile, Error> registryFile(const std::string& value);

    struct LoadedDeclarations {
        Json declarations = Json::object(), catalog = Json::object(), releases = Json::array();
    };
    using Fetch = std::function<void(std::string path, bool raw, Completion)>;
    void loadDeclarations(const DeclarationFile& registry,
                          const std::string& catalogPath,
                          const Url& base,
                          Fetch fetch,
                          std::function<void(std::variant<LoadedDeclarations, Error>)> done);
} // namespace coreci::github
