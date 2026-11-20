#include "StateSession.h"

#include <utility>

namespace coreci::persistence {
    StateSession::StateSession(StateStore& store,
                               Options options,
                               std::function<void(std::uint64_t)> changed,
                               std::function<bool()> operational)
        : store(store)
        , options(std::move(options))
        , changed(std::move(changed))
        , operational(std::move(operational)) {
    }
    bool StateSession::live(std::uint64_t value) const {
        return value == epoch && (!operational || operational());
    }
    void StateSession::start(std::function<void()> onReady, std::function<void(const StoreError&)> onError) {
        if (live(epoch)) {
            ready = std::move(onReady);
            error = std::move(onError);
            current.reset();
            const auto value = ++epoch;
            store.open(
                [this, value] {
                    if (live(value)) {
                        store.load(
                            [this, value](StoredState state) {
                                if (live(value)) {
                                    current = std::make_unique<WaveController>(options, std::move(state));
                                    ready();
                                }
                            },
                            [this, value](const StoreError& failure) {
                                if (live(value)) {
                                    failed(failure);
                                }
                            });
                    }
                },
                [this, value](const StoreError& failure) {
                    if (live(value)) {
                        failed(failure);
                    }
                });
        }
    }
    void StateSession::failed(const StoreError& failure) {
        ++epoch;
        current.reset();
        writing = false;
        auto reply = std::exchange(active, {});
        auto rejected = std::exchange(pending, {});
        if (reply) {
            reply(Unavailable{failure.message});
        }
        for (auto& item : rejected) {
            item.reply(Unavailable{failure.message});
        }
        error(failure);
    }
    void StateSession::recover(const std::string& reason) {
        if (live(epoch)) {
            failed({StoreError::Kind::Unavailable, reason});
        }
    }
    const WaveController* StateSession::engine() const {
        return live(epoch) ? current.get() : nullptr;
    }
    void StateSession::enqueue(Event input, Reply reply) {
        if (!engine()) {
            reply(Unavailable{"state unavailable during startup/recovery"});
        } else {
            pending.push_back({std::move(input), std::move(reply)});
            pump();
        }
    }
    void StateSession::pump() {
        if (!draining) {
            draining = true;
            while (engine() && !writing && !pending.empty()) {
                auto item = std::move(pending.front());
                pending.pop_front();
                const auto normalized = normalize(std::move(item.input));
                if (std::holds_alternative<Invalid>(normalized)) {
                    item.reply(std::get<Invalid>(normalized));
                } else {
                    const auto input = std::get<Event>(normalized);
                    const auto admission = current->classify(input);
                    if (std::holds_alternative<Ignore>(admission)) {
                        item.reply(Ignored{std::get<Ignore>(admission).reason});
                    } else {
                        auto candidate = std::make_shared<WaveController>(*current);
                        candidate->handle(input);
                        writing = true;
                        active = std::move(item.reply);
                        const auto value = epoch;
                        store.commit(
                            current->stored(),
                            candidate->stored(),
                            describe(input),
                            [this, candidate, value] {
                                if (live(value)) {
                                    current = std::make_unique<WaveController>(std::move(*candidate));
                                    auto reply = std::exchange(active, {});
                                    writing = false;
                                    changed(current->stored().sequence);
                                    reply(Accepted{});
                                    pump();
                                }
                            },
                            [this, value](const StoreError& failure) {
                                if (live(value)) {
                                    failed(failure);
                                }
                            });
                    }
                }
            }
            draining = false;
        }
    }
} // namespace coreci::persistence
