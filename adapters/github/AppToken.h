#pragma once
#include "HttpClient.h"
namespace coreci::github {
    // One authority for installation authentication; transport and clock are injected for tests.
    class AppToken {
    public:
        using Send = std::function<void(std::string, std::string, Completion, std::optional<std::string>, bool)>;
        AppToken(std::string appId,
                 std::string installationId,
                 std::string keyFile,
                 std::string apiPath,
                 Send send,
                 std::function<std::int64_t()> clock);
        void request(std::string path, Completion done, std::optional<std::string> body = {}, bool raw = false);

    private:
        void token(std::function<void(const std::string&)> done);
        void call(std::string path, Completion done, std::optional<std::string> body, bool raw, bool retried);
        std::string appId, installationId, keyFile, apiPath, cached;
        std::int64_t expires = 0;
        Send send;
        std::function<std::int64_t()> clock;
        std::vector<std::function<void(const std::string&)>> waiting;
    };
} // namespace coreci::github
