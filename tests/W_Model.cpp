#include "Fixture.h"
#include "adapters/dashboard/Model.h"

#include <nlohmann/json.hpp>
using namespace coreci;
using Json = nlohmann::json;

int main() {
    History h;
    h.load();
    h.apply(Tick{105});
    const auto w = h.wave();
    auto plan = Json::parse(app::planJson(w));
    CHECK_EQ(plan.size(), 4U);
    CHECK(plan["owner_run_id"].is_null());
    CHECK_EQ(plan["wave"], w.id);
    CHECK_EQ(plan["feed_branch"], "feed");
    CHECK_EQ(plan["targets"].size(), 2U);
    for (const auto& target : plan["targets"]) {
        CHECK_EQ(target.size(), 2U);
        for (const auto& unit : target["units"]) {
            CHECK_EQ(unit.size(), 10U);
            CHECK_EQ(unit["action"], "build");
            CHECK_EQ(unit["commit"], hash());
            CHECK_EQ(unit["repo"], "owner/" + unit["package"].get<std::string>());
            CHECK_EQ(unit["revision"], 1U);
        }
    }
    github::LoadedDeclarations declarations;
    declarations.pipeline = h.engine.pipeline();
    for (const auto& name : declarations.pipeline.targets()) {
        declarations.catalog[name] = {"debian", "13", "trixie", "amd64", "deb"};
    }
    auto model = Json::parse(dashboard::model(h.engine, declarations, 2, false));
    CHECK(!model["ready"].get<bool>());
    CHECK(model["units"].empty());
    h.apply(WaveStarted{106, w.id, 99, 3});
    plan["owner_run_id"] = 99;
    CHECK(Json::parse(app::planJson(h.wave())) == plan);
    const auto id = w.plan.at("t1")[0].buildId;
    h.apply(UnitBuildStarted{107, w.id, id});
    model = Json::parse(dashboard::model(h.engine, declarations, 3, true));
    CHECK_EQ(model["wave"]["phase"], "Running");
    CHECK_EQ(model["wave"]["started"], 106);
    CHECK_EQ(model["wave"]["html_url"], "https://github.com/SNodeC/Packages/actions/runs/99/attempts/3");
    auto unit = model["units"][0];
    CHECK_EQ(unit["status"], "Building");
    CHECK_EQ(unit["selection"]["version"], "1.0.0");
    CHECK_EQ(unit["build"]["start"], 107);
    CHECK(unit["build"]["end"].is_null());
    CHECK_EQ(unit["build"]["html_url"], model["wave"]["html_url"]);
    h.apply(UnitBuildSucceeded{110, w.id, id});
    h.apply(TargetFinished{110, w.id, "t1", "artifact"});
    const auto publication = h.publication();
    h.apply(PublicationStarted{111, publication.run.id, 202, 1});
    h.apply(UnitPublishSucceeded{112, publication.run.id, id, hash('c')});
    h.apply(PublicationFinished{113, publication.run.id, 202, "success"});
    h.apply(TargetFinished{114, w.id, "t2", ""});
    h.apply(WaveFinished{115, w.id, 99, "failure"});
    model = Json::parse(dashboard::model(h.engine, declarations, 4, true));
    unit = model["units"][0];
    CHECK_EQ(unit["status"], "Published");
    CHECK_EQ(unit["build"]["duration"], 3);
    CHECK_EQ(unit["publishedAt"], 112);
    CHECK_EQ(unit["published"]["feedCommit"], hash('c'));
    CHECK_EQ(unit["publish"]["start"], 111);
    CHECK_EQ(unit["publish"]["end"], 112);
    CHECK_EQ(unit["publish"]["html_url"], "https://github.com/SNodeC/Packages/actions/runs/202/attempts/1");
    CHECK_EQ(model["publication"]["publication"], publication.run.id);
    CHECK_EQ(model["publication"]["phase"], "Finished");
    CHECK_EQ(model["publication"]["items"].size(), 1U);
    CHECK_EQ(model["targets"][0]["status"], "Failed");
    CHECK_EQ(model["wave"]["finished"], 115);
    CHECK_EQ(model["wave"]["counts"]["Published"], 1);
    CHECK_EQ(model["wave"]["counts"]["Failed"], 5);
    CHECK_EQ(model["wave"]["counts"]["Blocked"], 5);
    CHECK(Json::parse(app::planJson(h.engine.waves().at(w.id))) == plan);
    const auto publicationPlan = Json::parse(app::publicationPlanJson(h.engine.publications().at(publication.run.id), h.engine.waves()));
    CHECK_EQ(publicationPlan.size(), 4U);
    CHECK_EQ(publicationPlan["owner_run_id"], 202);
    CHECK_EQ(publicationPlan["items"][0]["wave_run_id"], 99);
    CHECK_EQ(publicationPlan["items"][0]["units"][0]["build_id"], id);
    History late;
    late.load();
    late.apply(Tick{105});
    const auto lateWave = late.wave();
    const auto lateId = lateWave.plan.at("t1")[0].buildId;
    late.apply(UnitBuildSucceeded{106, lateWave.id, lateId});
    CHECK(!late.engine.units().at({"a", "t1"}).buildEnd);
    late.apply(WaveStarted{108, lateWave.id, 123, 2});
    late.apply(UnitBuildSucceeded{109, lateWave.id, lateId});
    CHECK_EQ(*late.engine.units().at({"a", "t1"}).buildEnd, 109);
    return checkResult();
}
