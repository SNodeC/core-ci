#pragma once
#include <map>
#include <set>
#include <string>
#include <vector>

namespace coreci {
    struct Repo {
        std::string owner, repo;
        bool operator==(const Repo&) const = default;
    };
    struct Workflow {
        std::string owner, repo, workflowFile, branch;
        bool operator==(const Workflow&) const = default;
    };
    struct Feed {
        std::string id;
        Workflow workflow;
    };
    enum class Family { Debian, Ubuntu, RaspberryPiOS, Fedora, RockyLinux, OpenWrtOpkg, OpenWrtApk };
    struct Target {
        std::string id;
        Family family;
        std::string feed;
    };
    struct Package {
        std::string name;
        Repo sourceRepo;
        Workflow buildWorkflow;
        std::set<std::string> targets;
        std::vector<std::string> prerequisites;
        bool acceptPrereleases = false;
    };
    struct Unit {
        std::string package, target;
        auto operator<=>(const Unit&) const = default;
    };
    class Pipeline {
    public:
        class Builder;
        const std::map<std::string, Feed>& feeds() const;
        const std::map<std::string, Target>& targets() const;
        const std::map<std::string, Package>& packages() const;
        const std::vector<std::string>& topologicalOrder(const std::string& target) const;
        const std::vector<std::string>& prerequisites(const std::string& package) const;
        std::vector<std::string> dependents(const std::string& package) const;
        std::vector<std::string> packagesForRepo(const std::string& owner, const std::string& repo) const;

    private:
        Pipeline() = default;
        std::map<std::string, Feed> feedMap;
        std::map<std::string, Target> targetMap;
        std::map<std::string, Package> packageMap;
        std::map<std::string, std::vector<std::string>> orders;
    };
    class Pipeline::Builder {
    public:
        Builder& feed(Feed value);
        Builder& target(Target value);
        Builder& package(Package value);
        Pipeline build() const;

    private:
        std::vector<Feed> feedList;
        std::vector<Target> targetList;
        std::vector<Package> packageList;
    };
} // namespace coreci
