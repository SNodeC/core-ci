#pragma once

#include "GraphStore.h"
#include "PipelineConfig.h"

namespace coreci {
    struct EnrollmentResult {
        int status = 200;
        Json body;
    };
    class Enrollment {
    public:
        using Callback = std::function<void(const EnrollmentResult&)>;
        Enrollment(GitHub& github, GraphStore& store, SourceReader& sources, const PipelineConfig& config);
        void list(const Callback& callback) const;
        void refresh(const Json& input, const Callback& callback);
        void save(const Json& input, const Callback& callback);
        void workflow(const Json& input, const Callback& callback);

    private:
        void persist(const Json& row, std::uint64_t generation, const Callback& callback, const Json& outcome);
        GitHub& github;
        GraphStore& store;
        SourceReader& sources;
        const PipelineConfig& config;
    };
} // namespace coreci
