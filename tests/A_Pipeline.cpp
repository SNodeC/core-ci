#include "Check.h"
#include "pipeline/Pipeline.h"
using namespace coreci;
int main() {
    const auto pipeline = productionPipeline();
    CHECK_EQ(pipeline.packages().size(), 2U);
    CHECK_EQ(pipeline.targets().size(), 76U);
    CHECK_EQ(pipeline.feeds().size(), 1U);
    CHECK(pipeline.prerequisites("snode.c").empty());
    CHECK((pipeline.prerequisites("mqttsuite") == std::vector<std::string>{"snode.c"}));
    std::map<Family, unsigned> counts;
    for (const auto& [id, target] : pipeline.targets()) {
        ++counts[target.family];
        CHECK_EQ(target.feed, "packages");
        CHECK((pipeline.topologicalOrder(id) == std::vector<std::string>{"snode.c", "mqttsuite"}));
    }
    CHECK_EQ(counts[Family::Debian], 12U);
    CHECK_EQ(counts[Family::Ubuntu], 4U);
    CHECK_EQ(counts[Family::Fedora], 4U);
    CHECK_EQ(counts[Family::RockyLinux], 4U);
    CHECK_EQ(counts[Family::RaspberryPiOS], 2U);
    CHECK_EQ(counts[Family::OpenWrtOpkg], 25U);
    CHECK_EQ(counts[Family::OpenWrtApk], 25U);
    for (const auto& [name, package] : pipeline.packages()) {
        CHECK_EQ(package.targets.size(), 76U);
        CHECK_EQ(package.sourceRepo.owner, "SNodeC");
        CHECK_EQ(package.sourceRepo.repo, name);
        CHECK_EQ(package.buildWorkflow.repo, "DistributionPackages");
        CHECK_EQ(package.buildWorkflow.branch, "main");
        CHECK_EQ(package.buildWorkflow.workflowFile, "coreci-build.yml");
    }
    const auto& workflow = pipeline.feeds().at("packages").workflow;
    CHECK_EQ(workflow.owner, "SNodeC");
    CHECK_EQ(workflow.repo, "DistributionPackages");
    CHECK_EQ(workflow.branch, "main");
    CHECK_EQ(workflow.workflowFile, "coreci-publish.yml");
    return checkResult();
}
