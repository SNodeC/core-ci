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
        Enrollment(GitHub& github, GraphStore& store, SourceReader& sources);
        Json list() const;
        void refresh(const Json& input, const Callback& callback);

    private:
        void persist(const Json& row, const Json& expected, const Callback& callback);
        GitHub& github;
        GraphStore& store;
        SourceReader& sources;
    };
} // namespace coreci
