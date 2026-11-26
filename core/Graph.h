#pragma once
#include <map>
#include <set>
#include <string>
#include <vector>

namespace coreci {
    std::string repoKey(const std::string& value);
    void validateIdentifier(const std::string& value, const std::string& kind);
    void validateRepository(const std::string& owner, const std::string& repo);
    bool validBranch(const std::string& value);
    struct Repo {
        std::string owner, repo;
        std::string branch = {};
        bool operator==(const Repo&) const = default;
    };
    bool sameIdentity(const Repo& a, const Repo& b);
    struct Package {
        std::string name;
        Repo sourceRepo;
        std::set<std::string> targets;
        std::map<std::string, std::string> prerequisites;
        unsigned revisionBase = 0;
        std::string ciCommit = {}, ciTree = {}, title = {};
        bool operator==(const Package&) const = default;
    };
    struct Unit {
        std::string package, target;
        auto operator<=>(const Unit&) const = default;
    };
    class Pipeline {
    public:
        class Builder;
        Pipeline() = default;
        bool operator==(const Pipeline&) const = default;
        const std::set<std::string>& targets() const;
        const std::map<std::string, Package>& packages() const;
        const std::vector<std::string>& topologicalOrder(const std::string& target) const;
        const std::map<std::string, std::string>& prerequisites(const std::string& package) const;
        bool containsRepository(const std::string& owner, const std::string& repo) const;

    private:
        std::set<std::string> targetMap;
        std::map<std::string, Package> packageMap;
        std::map<std::string, std::vector<std::string>> orders;
    };
    class Pipeline::Builder {
    public:
        Builder& target(std::string value);
        Builder& package(Package value);
        Pipeline build() const;

    private:
        std::set<std::string> targetList;
        std::vector<Package> packageList;
    };
} // namespace coreci
