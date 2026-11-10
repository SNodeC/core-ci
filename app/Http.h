#pragma once
#include "persistence/EventQueue.h"

#include <chrono>
#include <express/Request.h>
#include <memory>
#include <string>
namespace coreci::app {
    inline std::int64_t now() {
        return std::chrono::duration_cast<std::chrono::seconds>(std::chrono::system_clock::now().time_since_epoch()).count();
    }
    inline std::pair<int, std::string> response(const persistence::Result& result) {
        std::pair<int, std::string> output;
        std::visit(
            [&](const auto& r) {
                using T = std::decay_t<decltype(r)>;
                if constexpr (std::is_same_v<T, persistence::Accepted>) {
                    output = {200, "accepted"};
                } else if constexpr (std::is_same_v<T, persistence::Ignored>) {
                    output = {200, "ignored: " + r.reason};
                } else if constexpr (std::is_same_v<T, persistence::Invalid>) {
                    output = {400, "invalid: " + r.reason};
                } else {
                    output = {503, "unavailable"};
                }
            },
            result);
        return output;
    }
    inline bool contentType(const std::shared_ptr<express::Request>& req, const std::string& expected) {
        auto value = req->get("Content-Type");
        value = value.substr(0, value.find(';'));
        while (!value.empty() && (value.back() == ' ' || value.back() == '\t')) {
            value.pop_back();
        }
        for (char& c : value) {
            if (c >= 'A' && c <= 'Z') {
                c = static_cast<char>(c - 'A' + 'a');
            }
        }
        return value == expected;
    }
} // namespace coreci::app
