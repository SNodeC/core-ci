#ifndef CORE_CI_SOURCE_READER_H
#define CORE_CI_SOURCE_READER_H

#include "GitHub.h"
#include "Version.h"

#include <optional>

namespace coreci {
    struct SourceResult {
        int status = 200;
        nlohmann::json source;
        std::string content;
        std::string blobSha;
        std::string error;
    };

    class SourceReader {
    public:
        using Callback = std::function<void(const SourceResult&)>;
        explicit SourceReader(GitHub& github);
        void select(const std::string& repository, const std::string& tag, const Callback& callback);
        void definition(const std::string& repository, const std::string& commit, const Callback& callback);
        void file(const std::string& repository,
                  const std::string& commit,
                  const std::string& path,
                  const Callback& callback,
                  bool publicRepository = false,
                  const std::string& rootTree = "",
                  bool contents = true);
        static std::string blobSha(const std::string& content);

    private:
        struct Reading {
            std::string repository;
            bool publicRepository = false;
            bool contents = true;
            nlohmann::json source;
            std::string tag;
            std::vector<std::string> path;
            std::optional<Version> highest;
            bool ambiguous = false;
            unsigned int page = 1;
            Callback callback;
        };

        void request(const std::shared_ptr<Reading>& reading, const std::string& suffix, const GitHub::Callback& callback);
        static std::string decode(const nlohmann::json& blob, const std::string& sha);
        void tags(const std::shared_ptr<Reading>& reading);
        void reference(const std::shared_ptr<Reading>& reading);
        void peel(const std::shared_ptr<Reading>& reading, const std::string& type, const std::string& sha, unsigned int depth);
        void tree(const std::shared_ptr<Reading>& reading, const std::string& sha, unsigned int position);
        void blob(const std::shared_ptr<Reading>& reading, const std::string& sha);
        GitHub& github;
    };
} // namespace coreci

#endif
