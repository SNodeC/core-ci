#ifndef CORE_CI_GRAPH_H
#define CORE_CI_GRAPH_H

#include "Definition.h"

#include <cstdint>
#include <map>
#include <string>
#include <vector>

namespace coreci {

    struct Attempt {
        std::string id;
        std::uint64_t revision = 0;
        std::string state = "waiting";
        nlohmann::json assignment;
        nlohmann::json buildRun;
        nlohmann::json buildResult;
        nlohmann::json publicationRun;
        nlohmann::json receipt;
        std::string failure;
        bool operator==(const Attempt&) const = default;
    };

    struct Node {
        std::string id;
        nlohmann::json source;
        nlohmann::json target;
        nlohmann::json cause;
        std::vector<std::string> prerequisites;
        std::vector<Attempt> attempts;
        bool automatic = true;
        std::string supersededBy;
    };

    struct SelectedDefinition {
        std::uint64_t repositoryId = 0;
        std::string revision;
        std::string blobSha;
        nlohmann::json document;
    };

    struct GraphChange {
        std::vector<Node> nodes;
        std::uint64_t nextRevision = 0;
        std::vector<SelectedDefinition> definitions;
    };

    class Graph {
    public:
        void restore(const std::vector<Node>& records, std::uint64_t nextRevision);
        const std::map<std::string, Node>& nodes() const;
        std::uint64_t nextRebuildRevision() const;
        const Node& node(const std::string& id) const;
        std::vector<std::string> blockers(const std::string& id) const;
        GraphChange select(const std::string& root, const std::map<std::string, nlohmann::json>& sources, bool explicitRequest) const;
        GraphChange retry(const std::string& id) const;
        Node beginBuild(const std::string& id) const;
        Node bindRun(const std::string& id, const std::string& attemptId, bool publication, const nlohmann::json& run) const;
        Node built(const std::string& id, const std::string& attemptId, const nlohmann::json& result) const;
        Node beginPublication(const std::string& id) const;
        Node published(const std::string& id, const std::string& attemptId, const nlohmann::json& receipt) const;
        Node failed(const std::string& id, const std::string& attemptId, const std::string& reason) const;
        void validate(const GraphChange& change) const;
        void apply(const GraphChange& change);
        static void verifyDefinition(const Node& node, const nlohmann::json& document, const std::string& blobSha);
        static std::string digest(const nlohmann::json& value);

    private:
        std::map<std::string, Node> records;
        std::uint64_t nextRevision = 1;
    };

} // namespace coreci

#endif
