#include "app/App.h"
int main(int argc, char** argv) {
    coreci::Pipeline::Builder b;
    b.feed({"f1", {"owner", "feeds", "publish.yml", "stable"}})
        .feed({"f2", {"owner", "feeds", "publish.yml", "stable"}})
        .target({"t1", coreci::Family::Debian, "f1"})
        .target({"t2", coreci::Family::Fedora, "f2"})
        .package({"a", {"owner", "a"}, {"owner", "recipes", "build.yml", "trunk"}, {"t1", "t2"}, {}})
        .package({"b", {"owner", "b"}, {"owner", "recipes", "build.yml", "trunk"}, {"t1", "t2"}, {"a"}})
        .package({"c", {"owner", "c"}, {"owner", "recipes", "build.yml", "trunk"}, {"t1", "t2"}, {"a"}})
        .package({"d", {"owner", "d"}, {"owner", "recipes", "build.yml", "trunk"}, {"t1", "t2"}, {"b", "c"}})
        .package({"e", {"owner", "e"}, {"owner", "recipes", "build.yml", "trunk"}, {"t1"}, {"d"}});
    return coreci::app::run(argc, argv, b.build());
}
