#pragma once
#include "adapters/github/Declarations.h"
#include "auth/JwtAuthMiddleware.h"
#include "express/Router.h"
#include "persistence/EventQueue.h"

#include <chrono>
#include <core/timer/Timer.h>
namespace coreci::app {
    class Dashboard {
    public:
        using Res = std::shared_ptr<express::Response>;
        Dashboard(std::shared_ptr<snodec::auth::OAuthClient> client,
                  std::set<std::string> users,
                  persistence::EventQueue& queue,
                  std::function<void(std::function<void(bool)>)> reload,
                  std::function<std::uint64_t()> generation);
        ~Dashboard();
        void routes(const express::Router& router);
        void loaded(const github::LoadedDeclarations& value);
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
        persistence::EventQueue& queue;
        std::function<void(std::function<void(bool)>)> reload;
        std::function<std::uint64_t()> generation;
        std::vector<std::shared_ptr<Stream>> streams;
        github::LoadedDeclarations declarations;
        std::unique_ptr<core::timer::Timer> timer;
        std::string loadError;
        bool ready = false, dirty = true;
    };
} // namespace coreci::app
