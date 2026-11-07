#pragma once
#include "StatusJson.h"

#include <cstdint>
#include <map>
#include <optional>
#include <string>
#include <vector>
namespace coreci::app {
    // Non-persistent dispatch bookkeeping; snapshots are typed views, never retained JSON.
    class Presentation {
    public:
        void observe(const std::map<std::string, StatusSnapshot>& snapshots) {
            std::erase_if(feeds, [&](const auto& item) {
                return !snapshots.contains(item.first);
            });
            for (const auto& [id, body] : snapshots) {
                auto& feed = feeds[id];
                feed.dirty = feed.dirty || feed.body != body;
                feed.body = body;
            }
        }
        std::vector<std::string> due(std::int64_t now, std::int64_t interval) {
            std::vector<std::string> result;
            for (auto& [id, feed] : feeds) {
                if (feed.dirty && !feed.pending && (!feed.last || now - *feed.last >= interval)) {
                    feed.last = now;
                    feed.pending = true;
                    feed.sent = feed.body;
                    result.push_back(id);
                }
            }
            return result;
        }
        void complete(const std::string& id, bool accepted) {
            if (feeds.contains(id)) {
                auto& feed = feeds.at(id);
                feed.pending = false;
                feed.dirty = !accepted || feed.sent != feed.body;
            }
        }

    private:
        struct Feed {
            StatusSnapshot body, sent;
            std::optional<std::int64_t> last;
            bool dirty = true, pending = false;
        };
        std::map<std::string, Feed> feeds;
    };
} // namespace coreci::app
