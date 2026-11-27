#include "Graph.h"

#include <algorithm>
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
    bool sameIdentity(const Repo& a, const Repo& b) {
        return repoKey(a.owner) == repoKey(b.owner) && repoKey(a.repo) == repoKey(b.repo) && a.branch == b.branch;
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
        void repository(const std::string& owner, const std::string& repo, const std::string& branch) {
            validateRepository(owner, repo);
            if (!validBranch(branch)) {
                throw std::invalid_argument("invalid branch: " + branch);
            }
        }
    } // namespace
    void validateIdentifier(const std::string& value, const std::string& kind) {
        if (!characters(value, 64, "._-")) {
            throw std::invalid_argument("invalid " + kind + ": " + value);
        }
    }
    void validateRepository(const std::string& owner, const std::string& repo) {
        if (!characters(owner, 39, "-")) {
            throw std::invalid_argument("invalid owner: " + owner);
        }
        if (!characters(repo, 100, "._-") || repo == "." || repo == "..") {
            throw std::invalid_argument("invalid repository: " + repo);
        }
    }
    Pipeline::Builder& Pipeline::Builder::target(std::string value) {
        targetList.insert(std::move(value));
        return *this;
    }
    Pipeline::Builder& Pipeline::Builder::package(Package value) {
        packageList.push_back(std::move(value));
        return *this;
    }
    Pipeline Pipeline::Builder::build() const {
        Pipeline p;
        for (const auto& t : targetList) {
            validateIdentifier(t, "target");
            p.targetMap.insert(t);
        }
        for (const auto& pkg : packageList) {
            validateIdentifier(pkg.name, "package");
            if (pkg.revisionBase > 1000000) {
                throw std::invalid_argument("revision_base outside 0–1000000: " + pkg.name);
            }
            repository(pkg.sourceRepo.owner, pkg.sourceRepo.repo, pkg.sourceRepo.branch);
            if (!p.packageMap.emplace(pkg.name, pkg).second) {
                throw std::invalid_argument("duplicate package: " + pkg.name);
            }
        }
        std::map<std::string, std::string> owners;
        for (const auto& [name, pkg] : p.packageMap) {
            for (const auto& target : pkg.targets) {
                if (!p.targetMap.contains(target)) {
                    throw std::invalid_argument("unknown target: " + target + " for package " + name);
                }
            }
            for (const auto& [q, range] : pkg.prerequisites) {
                if (!p.packageMap.contains(q)) {
                    throw std::invalid_argument("unknown package: " + q + " required by " + name);
                }
                for (const auto& target : pkg.targets) {
                    if (!p.packageMap.at(q).targets.contains(target)) {
                        throw std::invalid_argument("prerequisite " + q + " missing target " + target + " required by " + name);
                    }
                }
            }
            const auto [owner, unique] = owners.emplace(repoKey(pkg.sourceRepo.owner + "/" + pkg.sourceRepo.repo), name);
            if (!unique) {
                throw std::invalid_argument("duplicate sourceRepo: " + owner->second + " and " + name);
            }
        }
        std::set<std::string> remaining;
        for (const auto& [name, pkg] : p.packageMap) {
            remaining.insert(name);
        }
        while (!remaining.empty()) {
            const auto next = std::find_if(remaining.begin(), remaining.end(), [&](const auto& name) {
                const auto& prerequisites = p.prerequisites(name);
                return std::none_of(prerequisites.begin(), prerequisites.end(), [&](const auto& q) {
                    return remaining.contains(q.first);
                });
            });
            if (next == remaining.end()) {
                std::vector<std::string> path;
                auto name = *remaining.begin();
                while (std::find(path.begin(), path.end(), name) == path.end()) {
                    path.push_back(name);
                    const auto& prerequisites = p.prerequisites(name);
                    name = std::find_if(prerequisites.begin(), prerequisites.end(), [&](const auto& q) {
                               return remaining.contains(q.first);
                           })->first;
                }
                std::string cycle = "cycle: ";
                for (auto it = std::find(path.begin(), path.end(), name); it != path.end(); ++it) {
                    cycle += *it + " -> ";
                }
                throw std::invalid_argument(cycle + name);
            }
            for (const auto& target : p.packageMap.at(*next).targets) {
                p.orders[target].push_back(*next);
            }
            remaining.erase(next);
        }
        std::erase_if(p.targetMap, [&](const auto& entry) {
            return !p.orders.contains(entry);
        });
        return p;
    }
    const std::set<std::string>& Pipeline::targets() const {
        return targetMap;
    }
    const std::map<std::string, Package>& Pipeline::packages() const {
        return packageMap;
    }
    const std::vector<std::string>& Pipeline::topologicalOrder(const std::string& target) const {
        return orders.at(target);
    }
    const std::map<std::string, std::string>& Pipeline::prerequisites(const std::string& package) const {
        return packageMap.at(package).prerequisites;
    }
    bool Pipeline::containsRepository(const std::string& owner, const std::string& repo) const {
        return std::any_of(packageMap.begin(), packageMap.end(), [&](const auto& entry) {
            return repoKey(entry.second.sourceRepo.owner) == repoKey(owner) && repoKey(entry.second.sourceRepo.repo) == repoKey(repo);
        });
    }
} // namespace coreci
