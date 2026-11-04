#include "EventQueue.h"

namespace coreci::persistence {
    EventQueue::EventQueue(EventStore& store,
                           std::function<Engine()> makeEngine,
                           std::function<Resumed()> resumed,
                           std::function<void(const std::vector<Command>&)> commands,
                           std::function<void(std::uint64_t, const std::string&)> warning)
        : store(store)
        , makeEngine(std::move(makeEngine))
        , resumed(std::move(resumed))
        , commands(std::move(commands))
        , warning(std::move(warning)) {
    }
    void EventQueue::start(std::function<void()> ready, std::function<void(const EventStore::StoreError&)> error) {
        onReady = std::move(ready);
        onError = std::move(error);
        rebuild();
    }
    void EventQueue::fatal(const EventStore::StoreError& error) {
        mode = Mode::Unavailable;
        current.reset();
        rejectPending(error.message);
        onError(error);
    }
    void EventQueue::rebuild() {
        mode = Mode::Loading;
        ++generation;
        current.reset();
        const auto epoch = generation;
        store.open(
            [this, epoch] {
                if (epoch == generation) {
                    current = std::make_unique<Engine>(makeEngine());
                    store.loadAll(
                        [this, epoch](std::uint64_t seq, const Event& event) {
                            if (epoch == generation) {
                                const auto classification = current->classify(event);
                                if (std::holds_alternative<Ignore>(classification)) {
                                    warning(seq, std::get<Ignore>(classification).reason);
                                } else {
                                    current->handle(event);
                                }
                            }
                        },
                        [this, epoch] {
                            if (epoch == generation) {
                                mode = Mode::Resuming;
                                pending.push_back({resumed(), [this, epoch](Result result) {
                                                       if (epoch == generation && std::holds_alternative<Accepted>(result)) {
                                                           mode = Mode::Ready;
                                                           onReady();
                                                       }
                                                   }});
                                pump();
                            }
                        },
                        [this, epoch](const EventStore::StoreError& error) {
                            if (epoch == generation) {
                                fatal(error);
                            }
                        });
                }
            },
            [this, epoch](const EventStore::StoreError& error) {
                if (epoch == generation) {
                    fatal(error);
                }
            });
    }

    void EventQueue::enqueue(Event event, Reply reply) {
        if (mode != Mode::Ready) {
            reply(Unavailable{"engine unavailable during startup/recovery"});
        } else {
            pending.push_back({std::move(event), std::move(reply)});
            pump();
        }
    }
    void EventQueue::rejectPending(const std::string& error) {
        auto rejected = std::move(pending);
        pending.clear();
        for (auto& item : rejected) {
            item.reply(Unavailable{error});
        }
    }
    void EventQueue::pump() {
        if (!draining) {
            draining = true;
            while ((mode == Mode::Ready || mode == Mode::Resuming) && !writing && !pending.empty()) {
                auto item = std::move(pending.front());
                pending.pop_front();
                const auto normalized = normalize(std::move(item.event));
                if (std::holds_alternative<Invalid>(normalized)) {
                    item.reply(std::get<Invalid>(normalized));
                } else {
                    const auto event = std::get<Event>(normalized);
                    const auto classification = current->classify(event);
                    if (std::holds_alternative<Ignore>(classification)) {
                        item.reply(Ignored{std::get<Ignore>(classification).reason});
                    } else {
                        writing = true;
                        activeReply = std::move(item.reply);
                        store.append(
                            event,
                            [this, event, epoch = generation] {
                                if (epoch == generation) {
                                    auto reply = std::move(activeReply);
                                    const auto emitted = current->handle(event);
                                    if (!emitted.empty()) {
                                        commands(emitted);
                                    }
                                    reply(Accepted{});
                                    writing = false;
                                    pump();
                                }
                            },
                            [this, epoch = generation](const std::string& error) {
                                if (epoch == generation) {
                                    recover(error);
                                }
                            });
                    }
                }
            }
            draining = false;
        }
    }
    void EventQueue::recover(const std::string& reason) {
        mode = Mode::Loading;
        current.reset();
        writing = false;
        auto reply = std::move(activeReply);
        if (reply) {
            reply(Unavailable{reason});
        }
        rejectPending(reason);
        rebuild();
    }
    const Engine* EventQueue::engine() const {
        return mode == Mode::Ready ? current.get() : nullptr;
    }
} // namespace coreci::persistence
