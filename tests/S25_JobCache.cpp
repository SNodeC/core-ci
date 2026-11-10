#include "Check.h"
#include "Fixture.h"
#include "adapters/dashboard/Model.h"
#include "app/JobCache.h"

#include <algorithm>
#include <deque>
using namespace coreci;
using namespace coreci::github;
int main() {
    auto engine = fixtureEngine(testPipeline(), {100, 50, 8, {}});
    engine.handle(TagObserved{1, "b", "v1.0.0", std::string(40, 'a'), Origin::Webhook});
    CHECK(dashboard::jobRequests(engine).empty());
    engine.handle(TagObserved{2, "a", "v1.0.0", std::string(40, 'a'), Origin::Webhook});
    const auto allocated = dashboard::jobRequests(engine);
    CHECK_EQ(allocated.size(), 2U);
    CHECK(std::all_of(allocated.begin(), allocated.end(), [](const auto& item) {
        return item.mainJob == "build";
    }));
    app::JobCache cache;
    const Url base{"api.example", ""};
    const JobRequest request{{"o", "r", "build.yml", "main"}, "a~t~1.0.0~r1", "build", 12};
    std::vector<std::string> paths;
    int changed = 0;
    std::string status = "in_progress";
    const Fetch fetch = [&](std::string path, bool raw, Completion done) {
        CHECK(!raw);
        paths.push_back(path);
        if (path.find("/jobs") != std::string::npos) {
            CHECK(path.find("/attempts/2/") != std::string::npos);
            done(
                200,
                R"({"jobs":[{"name":"resolve"},{"name":"build","status":"in_progress","html_url":"https://github.com/o/r/job/1","started_at":"2026-01-01T00:00:00Z","completed_at":null,"conclusion":null}]})",
                {});
        } else {
            const auto run =
                "{\"id\":12,\"run_attempt\":2,\"path\":\".github/workflows/build.yml\",\"display_title\":\"a~t~1.0.0~r1\",\"status\":\"" +
                status + "\",\"html_url\":\"https://github.com/o/r/actions/runs/12\"}";
            done(200, path.find('?') == std::string::npos ? run : "{\"workflow_runs\":[" + run + "]}", {});
        }
    };
    const auto notify = [&] {
        ++changed;
    };
    cache.refresh({request}, 100, base, fetch, notify);
    REQUIRE(cache.get(request));
    CHECK_EQ(cache.get(request)->runId, 12U);
    CHECK_EQ(cache.get(request)->job->status, "in_progress");
    CHECK_EQ(paths.size(), 2U);
    cache.refresh({request}, 129, base, fetch, notify);
    CHECK_EQ(paths.size(), 2U);
    status = "completed";
    cache.refresh({request}, 130, base, fetch, notify);
    CHECK_EQ(paths.size(), 4U);
    cache.refresh({request}, 160, base, fetch, notify);
    CHECK_EQ(paths.size(), 4U);
    app::JobCache names;
    auto unnamed = request;
    unnamed.runId.reset();
    names.refresh({unnamed}, 100, base, fetch, notify);
    REQUIRE(names.get(unnamed));
    CHECK_EQ(names.get(unnamed)->runId, 12U);
    CHECK(paths[4].ends_with("/actions/runs?per_page=100"));
    app::JobCache bounded;
    std::deque<Completion> pending;
    std::vector<JobRequest> many;
    for (unsigned i = 0; i < 10; ++i) {
        auto item = request;
        item.buildId += std::to_string(i);
        many.push_back(item);
    }
    const Fetch hold = [&](std::string, bool, Completion done) {
        pending.push_back(std::move(done));
    };
    bounded.refresh(many, 100, base, hold, notify);
    CHECK_EQ(pending.size(), 4U);
    bounded.refresh(many, 101, base, hold, notify);
    CHECK_EQ(pending.size(), 4U);
    unsigned completed = 0;
    while (!pending.empty()) {
        auto done = std::move(pending.front());
        pending.pop_front();
        done(500, "", {});
        ++completed;
        CHECK(pending.size() <= 4U);
    }
    CHECK_EQ(completed, 10U);
    CHECK(!bounded.get(many.front()));
    bounded.refresh(many, 101, base, hold, notify);
    CHECK(pending.empty());
    for (auto& done : pending) {
        done(200, "malformed", {});
    }
    CHECK(!bounded.get(many[4]));
    // Errors clear a previous running observation rather than displaying stale fields.
    app::JobCache errors;
    status = "in_progress";
    errors.refresh({request}, 100, base, fetch, notify);
    REQUIRE(errors.get(request));
    errors.refresh(
        {request},
        130,
        base,
        [](std::string, bool, Completion done) {
            done(503, "", {});
        },
        notify);
    CHECK(!errors.get(request));
    errors.refresh(
        {request},
        160,
        base,
        [](std::string, bool, Completion done) {
            done(200, "malformed", {});
        },
        notify);
    CHECK(!errors.get(request));
    return checkResult();
}
