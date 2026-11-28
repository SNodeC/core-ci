#pragma once
#include "adapters/github/Declarations.h"
#include "auth/JwtAuthMiddleware.h"
#include "express/Router.h"
#include "persistence/StateSession.h"

#include <chrono>
#include <core/timer/Timer.h>
#include <set>
namespace coreci::app {
    class Dashboard {
    public:
        using Res = std::shared_ptr<express::Response>;
        Dashboard(std::shared_ptr<snodec::auth::OAuthClient> client,
                  std::set<std::string> users,
                  persistence::StateSession& queue,
                  std::function<void(std::function<void(bool)>)> reload,
                  std::function<void(const Json&, persistence::StateSession::Reply)> admit);
        ~Dashboard();
        void routes(const express::Router& router);
        void changed();
        void failed(const std::string& message);

    private:
        struct Stream {
            Res res;
            std::string token, email;
            std::chrono::steady_clock::time_point keepAlive, checked;
            bool snapshot = true, pending = false, bearer = false, authorized = true;
        };
        void tick();
        std::shared_ptr<snodec::auth::OAuthClient> client;
        std::set<std::string> users;
        persistence::StateSession& queue;
        std::function<void(std::function<void(bool)>)> reload;
        std::function<void(const Json&, persistence::StateSession::Reply)> admit;
        std::vector<std::shared_ptr<Stream>> streams;
        std::unique_ptr<core::timer::Timer> timer;
        std::string loadError;
        bool dirty = true;
    };
} // namespace coreci::app
