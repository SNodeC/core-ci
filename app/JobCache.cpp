#include "JobCache.h"
namespace coreci::app {
    void JobCache::refresh(const std::vector<github::JobRequest>& requests,
                           std::int64_t now,
                           const github::Url& base,
                           github::Fetch fetch,
                           std::function<void()> changed) {
        if (!draining) {
            draining = true;
            for (const auto& request : requests) {
                auto& entry = entries[request.key()];
                if (pending < 4 && !entry.pending && now >= entry.next && (!entry.value || entry.value->status != "completed")) {
                    entry.pending = true;
                    entry.next = now + 30;
                    ++pending;
                    github::loadJob(request, base, fetch, [this, request, requests, now, base, fetch, changed](auto value) {
                        auto& entry = entries.at(request.key());
                        const bool different = entry.value != value;
                        entry.value = std::move(value);
                        entry.pending = false;
                        --pending;
                        if (different) {
                            changed();
                        }
                        refresh(requests, now, base, fetch, changed);
                    });
                }
            }
            draining = false;
        }
    }
    std::optional<github::RunJob> JobCache::get(const github::JobRequest& request) const {
        const auto found = entries.find(request.key());
        return found == entries.end() ? std::nullopt : found->second.value;
    }
} // namespace coreci::app
