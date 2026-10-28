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
        Json list() const;
        void refresh(const Json& input, const Callback& callback, const std::string& commit = "", const Json& expected = nullptr);
        void save(const Json& input, const Callback& callback);
        void workflow(const Json& input, const Callback& callback);

    private:
        void persist(const Json& row, const Json& expected, const Callback& callback);
        GitHub& github;
        GraphStore& store;
        SourceReader& sources;
        const PipelineConfig& config;
    };
} // namespace coreci
