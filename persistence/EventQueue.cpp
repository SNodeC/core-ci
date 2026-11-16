#include "EventQueue.h"

namespace coreci::persistence {
    EventQueue::EventQueue(EventStore& store,
                           std::function<Engine()> makeEngine,
                           std::function<Resumed()> resumed,
                           std::function<void(std::uint64_t, const std::string&)> warning,
                           std::function<void(std::uint64_t, const Event&, const Engine&)> committed,
                           std::function<bool()> operational)
        : store(store)
        , makeEngine(std::move(makeEngine))
        , resumed(std::move(resumed))
        , warning(std::move(warning))
        , committed(std::move(committed))
        , operational(std::move(operational)) {
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
        if (isCurrent(generation)) {
            mode = Mode::Loading;
            ++generation;
            current.reset();
            const auto epoch = generation;
            store.open(
                [this, epoch] {
                    if (isCurrent(epoch)) {
                        current = std::make_unique<Engine>(makeEngine());
                        store.loadAll(
                            [this, epoch](std::uint64_t seq, const Event& event) {
                                if (isCurrent(epoch)) {
                                    const auto classification = current->classify(event);
                                    if (std::holds_alternative<Ignore>(classification)) {
                                        warning(seq, std::get<Ignore>(classification).reason);
                                    } else {
                                        current->handle(event, seq);
                                    }
                                    if (committed) {
                                        committed(seq, event, *current);
                                    }
                                }
                            },
                            [this, epoch] {
                                if (isCurrent(epoch)) {
                                    mode = Mode::Resuming;
                                    pending.push_back({resumed(), [this, epoch](Result result) {
                                                           if (isCurrent(epoch) && std::holds_alternative<Accepted>(result)) {
                                                               mode = Mode::Ready;
                                                               onReady();
                                                           }
                                                       }});
                                    pump();
                                }
                            },
                            [this, epoch](const EventStore::StoreError& error) {
                                if (isCurrent(epoch)) {
                                    fatal(error);
                                }
                            });
                    }
                },
                [this, epoch](const EventStore::StoreError& error) {
                    if (isCurrent(epoch)) {
                        fatal(error);
                    }
                });
        }
    }

    void EventQueue::enqueue(Event event, Reply reply) {
        if (!engine()) {
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
            while (isCurrent(generation) && (mode == Mode::Ready || mode == Mode::Resuming) && !writing && !pending.empty()) {
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
                            [this, event, epoch = generation](std::uint64_t seq) {
                                if (isCurrent(epoch)) {
                                    auto reply = std::move(activeReply);
                                    current->handle(event, seq);
                                    if (committed) {
                                        committed(seq, event, *current);
                                    }
                                    reply(Accepted{});
                                    writing = false;
                                    pump();
                                }
                            },
                            [this, epoch = generation](const std::string& error) {
                                if (isCurrent(epoch)) {
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
        if (isCurrent(generation)) {
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
    }
    const Engine* EventQueue::engine() const {
        return mode == Mode::Ready && isCurrent(generation) ? current.get() : nullptr;
    }
    bool EventQueue::isCurrent(std::uint64_t epoch) const {
        return epoch == generation && (!operational || operational());
    }
} // namespace coreci::persistence
