#include "Graph.h"

#include <algorithm>
#include <functional>
#include <stdexcept>

namespace coreci {
    Pipeline::Builder& Pipeline::Builder::feed(Feed value) {
        feedList.push_back(std::move(value));
        return *this;
    }
    Pipeline::Builder& Pipeline::Builder::target(Target value) {
        targetList.push_back(std::move(value));
        return *this;
    }
    Pipeline::Builder& Pipeline::Builder::package(Package value) {
        packageList.push_back(std::move(value));
        return *this;
    }
    Pipeline Pipeline::Builder::build() const {
        Pipeline p;
        for (const auto& f : feedList) {
            if (!p.feedMap.emplace(f.id, f).second) {
                throw std::invalid_argument("duplicate feed: " + f.id);
            }
        }
        for (const auto& t : targetList) {
            if (!p.targetMap.emplace(t.id, t).second) {
                throw std::invalid_argument("duplicate target: " + t.id);
            }
            if (!p.feedMap.contains(t.feed)) {
                throw std::invalid_argument("unknown feed: " + t.feed + " for target " + t.id);
            }
        }
        for (const auto& pkg : packageList) {
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
                if (name < other && pkg.sourceRepo == q.sourceRepo) {
                    for (const auto& t : pkg.targets) {
                        if (q.targets.contains(t)) {
                            throw std::invalid_argument("sourceRepo overlap: " + name + " and " + other + " on " + t);
                        }
                    }
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
            if (pkg.sourceRepo == Repo{owner, repo}) {
                result.push_back(name);
            }
        }
        return result;
    }
} // namespace coreci
