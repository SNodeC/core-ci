#pragma once
#include "HttpClient.h"
namespace coreci::github {
    struct DeclarationFile {
        std::string owner, repo, path;
    };
    std::variant<DeclarationFile, Error> registryFile(const std::string& value);
    struct LoadedDeclarations {
        Pipeline pipeline;
    };
    using Fetch = std::function<void(Request, std::function<void(Response)>)>;
    void loadDeclarations(const DeclarationFile& registry,
                          const std::string& apiPath,
                          const std::string& token,
                          Fetch fetch,
                          std::function<void(std::variant<LoadedDeclarations, Error>)> done);
} // namespace coreci::github
