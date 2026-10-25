#ifndef CORE_CI_ENROLLMENT_H
#define CORE_CI_ENROLLMENT_H

#include "Database.h"
#include "Definition.h"
#include "GitHub.h"
#include "GraphStore.h"

#include <functional>
#include <map>
#include <string>

namespace coreci {

    struct EnrollmentResult {
        int status = 200;
        nlohmann::json body;
    };

    class Enrollment {
    public:
        using Callback = std::function<void(const EnrollmentResult&)>;
        Enrollment(Database& database, GitHub& github, GraphStore& graphStore);
        void restore(const Callback& callback);
        void list(const Callback& callback);
        void save(const nlohmann::json& request, const Callback& callback);
        void refresh(const nlohmann::json& request, const Callback& callback);

    private:
        struct Save {
            std::string repository;
            std::string name;
            std::string branch;
            std::string expectedRevision;
            std::string blob;
            std::string head;
            std::uint64_t id = 0;
            std::uint64_t generation = 0;
            Definition definition;
            bool importing = false;
            Callback callback;
        };

        void readHead(const std::shared_ptr<Save>& save);
        void readFile(const std::shared_ptr<Save>& save);
        void checkClaims(const std::shared_ptr<Save>& save, const std::function<void()>& next);
        void writeDefinition(const std::shared_ptr<Save>& save);
        void commit(const std::shared_ptr<Save>& save);
        void remember(const std::shared_ptr<Save>& save, const GitHubResult& result);
        void finish(const std::shared_ptr<Save>& save, int status, const nlohmann::json& body);

        Database& database;
        GitHub& github;
        GraphStore& graphStore;
        bool saving = false;
    };

} // namespace coreci

#endif
