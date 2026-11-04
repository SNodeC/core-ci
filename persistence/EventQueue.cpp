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
    void EventQueue::start(std::function<void()> ready, std::function<void(const std::string&)> error) {
        onReady = std::move(ready);
        onError = std::move(error);
        rebuild();
    }
    void EventQueue::fatal(const std::string& error) {
        mode = Mode::Unavailable;
        current.reset();
        rejectPending(error);
        onError(error);
    }
    void EventQueue::rebuild() {
        mode = Mode::Loading;
        current.reset();
        store.open(
            [this] {
                current = std::make_unique<Engine>(makeEngine());
                store.loadAll(
                    [this](std::uint64_t seq, const Event& event) {
                        const auto classification = current->classify(event);
                        if (std::holds_alternative<Ignore>(classification)) {
                            warning(seq, std::get<Ignore>(classification).reason);
                        } else {
                            current->handle(event);
                        }
                    },
                    [this] {
                        mode = Mode::Resuming;
                        pending.push_back({resumed(), [this](Result result) {
                                               if (std::holds_alternative<Accepted>(result)) {
                                                   mode = Mode::Ready;
                                                   onReady();
                                               }
                                           }});
                        pump();
                    },
                    [this](const std::string& error) {
                        fatal(error);
                    });
            },
            [this](const std::string& error) {
                fatal(error);
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
                        store.append(
                            event,
                            [this, event, reply = item.reply] {
                                const auto emitted = current->handle(event);
                                if (!emitted.empty()) {
                                    commands(emitted);
                                }
                                reply(Accepted{});
                                writing = false;
                                pump();
                            },
                            [this, reply = item.reply](const std::string& error) {
                                mode = Mode::Loading;
                                current.reset();
                                writing = false;
                                reply(Unavailable{error});
                                rejectPending(error);
                                rebuild();
                            });
                    }
                }
            }
            draining = false;
        }
    }
    const Engine* EventQueue::engine() const {
        return mode == Mode::Ready ? current.get() : nullptr;
    }
} // namespace coreci::persistence
