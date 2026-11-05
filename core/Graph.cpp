#include "Graph.h"

#include <algorithm>
#include <functional>
#include <stdexcept>

namespace coreci {
    std::string repoKey(const std::string& value) {
        std::string result = value;
        for (auto& c : result) {
            if (c >= 'A' && c <= 'Z') {
                c += 'a' - 'A';
            }
        }
        return result;
    }
    bool Repo::operator==(const Repo& other) const {
        return repoKey(owner) == repoKey(other.owner) && repoKey(repo) == repoKey(other.repo) && branch == other.branch;
    }
    bool Workflow::operator==(const Workflow& other) const {
        return repoKey(owner) == repoKey(other.owner) && repoKey(repo) == repoKey(other.repo) && workflowFile == other.workflowFile &&
               branch == other.branch;
    }
    bool validBranch(const std::string& value) {
        bool valid = !value.empty() &&
                     std::count_if(value.begin(),
                                   value.end(),
                                   [](unsigned char c) {
                                       return (c & 0xc0) != 0x80;
                                   }) <= 255 &&
                     value != "@" && value != "HEAD" && value.front() != '-' && value.front() != '/' && value.back() != '/' &&
                     value.back() != '.' && value.find("..") == std::string::npos && value.find("@{") == std::string::npos &&
                     value.find("//") == std::string::npos;
        std::size_t start = 0;
        for (std::size_t i = 0; valid && i <= value.size(); ++i) {
            if (i == value.size() || value[i] == '/') {
                const auto part = value.substr(start, i - start);
                valid = !part.empty() && part.front() != '.' && !part.ends_with(".lock");
                start = i + 1;
            } else {
                const unsigned char c = value[i];
                valid = c > 32 && c != 127 && std::string("~^:?*[\\").find(c) == std::string::npos;
            }
        }
        return valid;
    }
    namespace {
        bool characters(const std::string& value, std::size_t limit, const std::string& extra) {
            return !value.empty() && value.size() <= limit && std::all_of(value.begin(), value.end(), [&](unsigned char c) {
                return (c >= 'a' && c <= 'z') || (c >= 'A' && c <= 'Z') || (c >= '0' && c <= '9') || extra.find(c) != std::string::npos;
            });
        }
        void identifier(const std::string& value, const std::string& kind) {
            if (!characters(value, 64, "._-")) {
                throw std::invalid_argument("invalid " + kind + ": " + value);
            }
        }
        void repository(const std::string& owner, const std::string& repo, const std::string& branch) {
            validateRepository(owner, repo);
            if (!validBranch(branch)) {
                throw std::invalid_argument("invalid branch: " + branch);
            }
        }
        void workflow(const Workflow& w) {
            repository(w.owner, w.repo, w.branch);
            validateWorkflowFile(w.workflowFile);
        }
    } // namespace
    void validateRepository(const std::string& owner, const std::string& repo) {
        if (!characters(owner, 39, "-")) {
            throw std::invalid_argument("invalid owner: " + owner);
        }
        if (!characters(repo, 100, "._-") || repo == "." || repo == "..") {
            throw std::invalid_argument("invalid repository: " + repo);
        }
    }
    void validateWorkflowFile(const std::string& file) {
        if (!characters(file, 255, "._-") || !(file.ends_with(".yml") || file.ends_with(".yaml"))) {
            throw std::invalid_argument("invalid workflow: " + file);
        }
    }
    std::set<std::pair<std::string, std::string>> Pipeline::repositories() const {
        std::set<std::pair<std::string, std::string>> result;
        for (const auto& [name, pkg] : packageMap) {
            result.emplace(repoKey(pkg.sourceRepo.owner), repoKey(pkg.sourceRepo.repo));
        }
        return result;
    }

    Pipeline::Builder& Pipeline::Builder::feed(Feed value) {
        value.workflow.owner = repoKey(value.workflow.owner);
        value.workflow.repo = repoKey(value.workflow.repo);
        feedList.push_back(std::move(value));
        return *this;
    }
    Pipeline::Builder& Pipeline::Builder::target(Target value) {
        targetList.push_back(std::move(value));
        return *this;
    }
    Pipeline::Builder& Pipeline::Builder::package(Package value) {
        value.sourceRepo.owner = repoKey(value.sourceRepo.owner);
        value.sourceRepo.repo = repoKey(value.sourceRepo.repo);
        value.buildWorkflow.owner = repoKey(value.buildWorkflow.owner);
        value.buildWorkflow.repo = repoKey(value.buildWorkflow.repo);
        std::sort(value.prerequisites.begin(), value.prerequisites.end());
        value.prerequisites.erase(std::unique(value.prerequisites.begin(), value.prerequisites.end()), value.prerequisites.end());
        packageList.push_back(std::move(value));
        return *this;
    }
    Pipeline Pipeline::Builder::build() const {
        Pipeline p;
        for (const auto& f : feedList) {
            identifier(f.id, "feed");
            workflow(f.workflow);
            if (!p.feedMap.emplace(f.id, f).second) {
                throw std::invalid_argument("duplicate feed: " + f.id);
            }
        }
        for (const auto& t : targetList) {
            identifier(t.id, "target");
            if (!p.targetMap.emplace(t.id, t).second && p.targetMap.at(t.id).feed != t.feed) {
                throw std::invalid_argument("target has different feeds: " + t.id);
            }
            if (!p.feedMap.contains(t.feed)) {
                throw std::invalid_argument("unknown feed: " + t.feed + " for target " + t.id);
            }
        }
        for (const auto& pkg : packageList) {
            identifier(pkg.name, "package");
            repository(pkg.sourceRepo.owner, pkg.sourceRepo.repo, pkg.sourceRepo.branch);
            workflow(pkg.buildWorkflow);
            if (!p.packageMap.emplace(pkg.name, pkg).second) {
                throw std::invalid_argument("duplicate package: " + pkg.name);
            }
        }
        for (const auto& [name, pkg] : p.packageMap) {
            for (const auto& target : pkg.targets) {
                if (!p.targetMap.contains(target)) {
                    throw std::invalid_argument("unknown target: " + target + " for package " + name);
                }
            }
            for (const auto& q : pkg.prerequisites) {
                if (!p.packageMap.contains(q)) {
                    throw std::invalid_argument("unknown package: " + q + " required by " + name);
                }
                for (const auto& target : pkg.targets) {
                    if (!p.packageMap.at(q).targets.contains(target)) {
                        throw std::invalid_argument("prerequisite " + q + " missing target " + target + " required by " + name);
                    }
                }
            }
            for (const auto& [other, q] : p.packageMap) {
                if (name < other && repoKey(pkg.sourceRepo.owner) == repoKey(q.sourceRepo.owner) &&
                    repoKey(pkg.sourceRepo.repo) == repoKey(q.sourceRepo.repo)) {
                    throw std::invalid_argument("duplicate sourceRepo: " + name + " and " + other);
                }
            }
        }
        std::map<std::string, int> marks;
        std::vector<std::string> path;
        std::function<void(const std::string&)> visit = [&](const std::string& name) {
            if (marks[name] == 1) {
                std::string cycle = "cycle: ";
                for (auto it = std::find(path.begin(), path.end(), name); it != path.end(); ++it) {
                    cycle += *it + " -> ";
                }
                throw std::invalid_argument(cycle + name);
            } else if (marks[name] == 0) {
                marks[name] = 1;
                path.push_back(name);
                auto prerequisites = p.prerequisites(name);
                std::sort(prerequisites.begin(), prerequisites.end());
                for (const auto& q : prerequisites) {
                    visit(q);
                }
                path.pop_back();
                marks[name] = 2;
            }
        };
        for (const auto& [name, pkg] : p.packageMap) {
            visit(name);
        }
        std::erase_if(p.targetMap, [&](const auto& entry) {
            return std::none_of(p.packageMap.begin(), p.packageMap.end(), [&](const auto& pkg) {
                return pkg.second.targets.contains(entry.first);
            });
        });
        for (const auto& [target, definition] : p.targetMap) {
            std::set<std::string> remaining;
            for (const auto& [name, pkg] : p.packageMap) {
                if (pkg.targets.contains(target)) {
                    remaining.insert(name);
                }
            }
            auto& order = p.orders[target];
            while (!remaining.empty()) {
                auto next = std::find_if(remaining.begin(), remaining.end(), [&](const auto& name) {
                    const auto& prerequisites = p.prerequisites(name);
                    return std::none_of(prerequisites.begin(), prerequisites.end(), [&](const auto& q) {
                        return remaining.contains(q);
                    });
                });
                order.push_back(*next);
                remaining.erase(next);
            }
        }
        return p;
    }
    const std::map<std::string, Feed>& Pipeline::feeds() const {
        return feedMap;
    }
    const std::map<std::string, Target>& Pipeline::targets() const {
        return targetMap;
    }
    const std::map<std::string, Package>& Pipeline::packages() const {
        return packageMap;
    }
    const std::vector<std::string>& Pipeline::topologicalOrder(const std::string& target) const {
        return orders.at(target);
    }
    const std::vector<std::string>& Pipeline::prerequisites(const std::string& package) const {
        return packageMap.at(package).prerequisites;
    }
    std::vector<std::string> Pipeline::dependents(const std::string& package) const {
        std::vector<std::string> result;
        for (const auto& [name, pkg] : packageMap) {
            if (std::find(pkg.prerequisites.begin(), pkg.prerequisites.end(), package) != pkg.prerequisites.end()) {
                result.push_back(name);
            }
        }
        return result;
    }
    std::vector<std::string> Pipeline::packagesForRepo(const std::string& owner, const std::string& repo) const {
        std::vector<std::string> result;
        for (const auto& [name, pkg] : packageMap) {
            if (repoKey(pkg.sourceRepo.owner) == repoKey(owner) && repoKey(pkg.sourceRepo.repo) == repoKey(repo)) {
                result.push_back(name);
            }
        }
        return result;
    }
} // namespace coreci
