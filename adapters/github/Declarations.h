#pragma once
#include "HttpClient.h"
namespace coreci::github {
    struct DeclarationFile {
        std::string owner, repo, path;
    };
    std::variant<DeclarationFile, Error> registryFile(const std::string& value);
    struct CatalogTarget {
        std::string distribution, suite, arch, format;
    };
    struct PackageDetails {
        std::string title, commit;
    };
    struct LoadedDeclarations {
        Pipeline pipeline;
        std::map<std::string, Workflow> presentations = {};
        std::map<std::string, CatalogTarget> catalog = {};
        std::map<std::string, PackageDetails> packages = {};
    };
    using Fetch = std::function<void(std::string path, bool raw, Completion)>;
    void loadDeclarations(const DeclarationFile& registry,
                          const std::string& apiPath,
                          Fetch fetch,
                          std::function<void(std::variant<LoadedDeclarations, Error>)> done);
} // namespace coreci::github
