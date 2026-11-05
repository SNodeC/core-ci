#pragma once
#include "core/Engine.h"
namespace coreci {
    inline Pipeline testPipeline() {
        Pipeline::Builder b;
        b.feed({"f1", {"owner", "feeds", "publish.yml", "stable"}})
            .feed({"f2", {"owner", "feeds", "publish.yml", "stable"}})
            .target({"t1", "f1"})
            .target({"t2", "f2"});
        b.package({"a", {"owner", "a", "trunk"}, {"owner", "recipes", "build.yml", "trunk"}, {"t1", "t2"}, {}})
            .package({"b", {"owner", "b", "trunk"}, {"owner", "recipes", "build.yml", "trunk"}, {"t1", "t2"}, {"a"}})
            .package({"c", {"owner", "c", "trunk"}, {"owner", "recipes", "build.yml", "trunk"}, {"t1", "t2"}, {"a"}})
            .package({"d", {"owner", "d", "trunk"}, {"owner", "recipes", "build.yml", "trunk"}, {"t1", "t2"}, {"b", "c"}})
            .package({"e", {"owner", "e", "trunk"}, {"owner", "recipes", "build.yml", "trunk"}, {"t1"}, {"d"}});
        return b.build();
    }
    inline Engine fixtureEngine(Pipeline pipeline, Options options) {
        Engine result(options);
        result.handle(GraphChanged{100, std::move(pipeline)});
        return result;
    }
} // namespace coreci
