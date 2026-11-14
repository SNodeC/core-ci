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
    struct Target {
        std::string id;
        bool operator==(const Target&) const = default;
    };
    struct Package {
        std::string name;
        Repo sourceRepo;
        std::set<std::string> targets;
        std::vector<std::string> prerequisites;
        unsigned revisionBase = 0;
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
        const std::map<std::string, Target>& targets() const;
        const std::map<std::string, Package>& packages() const;
        const std::vector<std::string>& topologicalOrder(const std::string& target) const;
        const std::vector<std::string>& prerequisites(const std::string& package) const;
        std::vector<std::string> packagesForRepo(const std::string& owner, const std::string& repo) const;

    private:
        std::map<std::string, Target> targetMap;
        std::map<std::string, Package> packageMap;
        std::map<std::string, std::vector<std::string>> orders;
    };
    class Pipeline::Builder {
    public:
        Builder& target(Target value);
        Builder& package(Package value);
        Pipeline build() const;

    private:
        std::vector<Target> targetList;
        std::vector<Package> packageList;
    };
} // namespace coreci
