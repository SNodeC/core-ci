#pragma once
#include "JobCache.h"
#include "auth/JwtAuthMiddleware.h"
#include "express/Router.h"
#include "persistence/EventQueue.h"

#include <core/timer/Timer.h>
namespace coreci::app {
    class Dashboard {
    public:
        using Res = std::shared_ptr<express::Response>;
        Dashboard(std::shared_ptr<snodec::auth::OAuthClient> client,
                  std::set<std::string> users,
                  persistence::EventQueue& queue,
                  github::Url base,
                  github::Fetch fetch,
                  std::function<void(std::function<void(bool)>)> reload,
                  std::function<std::uint64_t()> generation);
        ~Dashboard();
        void routes(const express::Router& router);
        void loaded(const github::LoadedDeclarations& value);
        void changed();

    private:
        struct Stream {
            Res res;
            std::string token, email;
            std::int64_t keepAlive = 0, checked = 0;
            bool snapshot = true, pending = false;
        };
        void tick();
        std::shared_ptr<snodec::auth::OAuthClient> client;
        std::set<std::string> users;
        persistence::EventQueue& queue;
        github::Url base;
        github::Fetch fetch;
        std::function<void(std::function<void(bool)>)> reload;
        std::function<std::uint64_t()> generation;
        std::vector<std::shared_ptr<Stream>> streams;
        JobCache jobs;
        github::LoadedDeclarations declarations;
        std::unique_ptr<core::timer::Timer> timer;
        bool ready = false, dirty = true;
    };
} // namespace coreci::app
