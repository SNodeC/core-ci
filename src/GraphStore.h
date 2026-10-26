#ifndef CORE_CI_GRAPH_STORE_H
#define CORE_CI_GRAPH_STORE_H

#include "Database.h"
#include "Graph.h"

#include <functional>

namespace coreci {
    class GraphStore {
    public:
        explicit GraphStore(Database& database);
        void restore(const Database::Callback& callback);
        void commit(const GraphChange& change, const Database::Callback& callback);
        void enroll(const std::string& name,
                    const std::string& repository,
                    const std::string& branch,
                    const SelectedDefinition& definition,
                    std::uint64_t expectedGeneration,
                    const Database::Callback& callback);
        std::uint64_t getGeneration() const;
        const Graph& graph() const;
        nlohmann::json view() const;
        nlohmann::json publicView() const;
        std::function<void(bool)> onChanged;

    private:
        struct Restoring {
            std::map<std::string, Node> nodes;
            std::uint64_t generation = 0;
            std::uint64_t nextRevision = 1;
            Database::Callback callback;
        };

        void readNodes(const std::shared_ptr<Restoring>& restoring);
        void readEdges(const std::shared_ptr<Restoring>& restoring);
        void readAttempts(const std::shared_ptr<Restoring>& restoring);
        std::string guard(std::uint64_t nextRevision, std::uint64_t expectedGeneration) const;

        Database& database;
        Graph state;
        std::uint64_t generation = 0;
    };
} // namespace coreci

#endif
