#pragma once
#include "persistence/StateSession.h"

#include <chrono>
#include <express/Request.h>
#include <express/Response.h>
#include <memory>
#include <string>
namespace coreci::app {
    using Req = std::shared_ptr<express::Request>;
    using Res = std::shared_ptr<express::Response>;
    inline void answer(const Res& res, int code, const std::string& text) {
        res->status(code).set("Content-Type", "text/plain").send(text);
    }
    inline void method(const Req&, const Res& res) {
        answer(res, 405, "method not allowed");
    }
    inline std::int64_t now() {
        return std::chrono::duration_cast<std::chrono::seconds>(std::chrono::system_clock::now().time_since_epoch()).count();
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
