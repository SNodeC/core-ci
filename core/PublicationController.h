#pragma once
#include "WaveController.h"

namespace coreci {
    class PublicationController {
    public:
        static bool classify(const WaveController& state, const Event& event);
        static void apply(WaveController& state, const Options& options, const Event& event);
        static void schedule(WaveController& state, const Options& options);
        static void advance(WaveController& state, const Options& options);
        static bool retry(WaveController& state, const Unit& unit);

    private:
        static void finish(WaveController& state, Publication& publication, const Options& options, const std::string& reason);
    };
} // namespace coreci
