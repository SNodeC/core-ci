#pragma once
#include "HttpClient.h"
namespace coreci::github {
    struct DeclarationFile {
        std::string owner, repo, path;
    };
    std::variant<DeclarationFile, Error> registryFile(const std::string& value);
    struct CatalogTarget {
        std::string distribution, release, codename, arch, family;
    };
    struct LoadedDeclarations {
        Pipeline pipeline;
        std::map<std::string, Tag> releases = {};
        std::map<std::string, CatalogTarget> catalog = {};
    };
    using Fetch = std::function<void(std::string path, bool raw, Completion)>;
    void loadDeclarations(const DeclarationFile& registry,
                          const std::string& catalogPath,
                          const Url& base,
                          Fetch fetch,
                          std::function<void(std::variant<LoadedDeclarations, Error>)> done);
} // namespace coreci::github
