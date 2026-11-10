#pragma once
#include "adapters/github/Jobs.h"
namespace coreci::app {
    class JobCache {
    public:
        void refresh(const std::vector<github::JobRequest>& requests,
                     std::int64_t now,
                     const github::Url& base,
                     github::Fetch fetch,
                     std::function<void()> changed);
        std::optional<github::RunJob> get(const github::JobRequest& request) const;

    private:
        struct Entry {
            std::optional<github::RunJob> value;
            std::int64_t next = 0;
            bool pending = false;
        };
        std::map<std::string, Entry> entries;
        std::size_t pending = 0;
        bool draining = false;
    };
} // namespace coreci::app
