#include "StateSession.h"

#include <utility>

namespace coreci::persistence {
    StateSession::StateSession(StateStore& store, Options options, std::function<void()> changed, std::function<bool()> operational)
        : store(store)
        , driver(std::move(options))
        , changed(std::move(changed))
        , operational(std::move(operational)) {
    }

    bool StateSession::live(std::uint64_t value) const {
        return value == epoch && operational();
    }

    void StateSession::start(std::function<void()> onReady, std::function<void(const StoreError&)> onError) {
        if (live(epoch)) {
            ready = std::move(onReady);
            error = std::move(onError);
            current.reset();
            const auto value = ++epoch;
            const auto problem = [this, value](const auto& reason) {
                if (live(value)) {
                    failed(reason);
                }
            };
            store.open(
                [this, value, problem] {
                    if (live(value)) {
                        store.load(
                            [this, value](auto loaded) {
                                if (live(value)) {
                                    current = std::make_unique<Json>(std::move(loaded));
                                    if (current->at("initialized") == false) {
                                        enqueue(
                                            "startup pause",
                                            [this](auto& state) {
                                                state["initialized"] = true;
                                                return driver.action(
                                                    state, "dispatch-paused", {{"paused", driver.options().paused.value_or(false)}});
                                            },
                                            [this](auto result) {
                                                if (result.code == 200) {
                                                    ready();
                                                }
                                            });
                                    } else {
                                        ready();
                                    }
                                }
                            },
                            problem);
                    }
                },
                problem);
        }
    }

    void StateSession::failed(const StoreError& failure) {
        ++epoch;
        current.reset();
        writing = false;
        auto reply = std::exchange(active, {});
        auto rejected = std::exchange(pending, {});
        if (reply) {
            reply({503, "unavailable"});
        }
        for (auto& item : rejected) {
            if (item.reply) {
                item.reply({503, "unavailable"});
            }
        }
        error(failure);
    }

    void StateSession::recover(const std::string& reason) {
        if (live(epoch)) {
            failed({StoreError::Kind::Unavailable, reason});
        }
    }

    const Json* StateSession::state() const {
        return live(epoch) ? current.get() : nullptr;
    }

    const Scheduler& StateSession::scheduler() const {
        return driver;
    }

    void StateSession::enqueue(std::string description, Transition apply, Reply reply) {
        if (state()) {
            pending.push_back({std::move(description), std::move(apply), std::move(reply)});
            pump();
        } else if (reply) {
            reply({503, "unavailable"});
        }
    }

    void StateSession::pump() {
        if (!draining) {
            draining = true;
            while (state() && !writing && !pending.empty()) {
                auto item = std::move(pending.front());
                pending.pop_front();
                auto candidate = std::make_shared<Json>(*current);
                Result result;
                try {
                    result = item.apply(*candidate);
                } catch (const std::exception&) {
                    result = {400, "invalid transition details"};
                    *candidate = *current;
                }
                if (*candidate == *current) {
                    if (item.reply) {
                        item.reply(result);
                    }
                } else {
                    writing = true;
                    active = std::move(item.reply);
                    const auto value = epoch;
                    store.commit(
                        *current,
                        *candidate,
                        item.description,
                        [this, candidate, value, result] {
                            if (live(value)) {
                                current = std::make_unique<Json>(std::move(*candidate));
                                auto reply = std::exchange(active, {});
                                writing = false;
                                changed();
                                if (reply) {
                                    reply(result);
                                }
                                pump();
                            }
                        },
                        [this, value](const auto& failure) {
                            if (live(value)) {
                                failed(failure);
                            }
                        });
                }
            }
            draining = false;
        }
    }
} // namespace coreci::persistence
