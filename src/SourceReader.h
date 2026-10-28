#pragma once

#include "GitHub.h"
#include "Version.h"

namespace coreci {
    struct SourceResult {
        int status = 200;
        Json source;
        std::string content;
        std::string blobSha;
        std::string error;
    };
    class SourceReader {
    public:
        using Callback = std::function<void(const SourceResult&)>;
        explicit SourceReader(GitHub& github)
            : github(github) {
        }
        void select(const std::string& repository, const std::string& tag, const Callback& callback, const std::string& floor = "");
        void definition(const std::string& repository, const std::string& ref, const Callback& callback);
        void file(const std::string& repository,
                  const std::string& ref,
                  const std::string& path,
                  const Callback& callback,
                  bool publicRepository = false,
                  const std::string& tree = "",
                  bool contents = true);
        static Json parseDefinition(const std::string& content);
        static std::string blobSha(const std::string& content);

    private:
        struct Reading {
            std::string repository;
            std::string tag;
            SourceResult result;
            std::vector<std::string> path;
            unsigned int page = 1;
            bool ambiguous = false;
            bool publicRepository = false;
            bool contents = true;
            Callback callback;
        };
        static void finish(const std::shared_ptr<Reading>& reading, const SourceResult& result);
        static std::vector<std::string> pathParts(const std::string& path);
        void read(const std::shared_ptr<Reading>& reading, const std::string& suffix, const GitHub::Callback& callback);
        void tags(const std::shared_ptr<Reading>& reading);
        void reference(const std::shared_ptr<Reading>& reading);
        void peel(const std::shared_ptr<Reading>& reading, const Json& object, unsigned int depth);
        void tree(const std::shared_ptr<Reading>& reading, const std::string& sha);
        GitHub& github;
    };
} // namespace coreci
