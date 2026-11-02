#include "Controller.h"

#include <filesystem>
#include <fstream>
#include <iostream>

int main(int argc, char* argv[]) {
    int result = 0;
    try {
        ci::require(argc == 3, "Fixture graph and archive directory required");
        std::ifstream input(argv[1]);
        const auto graph = ci::Json::parse(input);
        const auto& active = graph.at("executions").at(0).at("attempts").back();
        unsigned int checked = 0;
        for (const auto& entry : std::filesystem::directory_iterator(argv[2])) {
            if (entry.path().extension() == ".zip") {
                std::ifstream archive(entry.path(), std::ios::binary);
                const std::string bytes{std::istreambuf_iterator<char>(archive), std::istreambuf_iterator<char>()};
                bool accepted = true;
                try {
                    const auto manifest = ci::artifactManifest(bytes, active.at("assignment"));
                    ci::require(manifest.at("attempt") == active.at("id"), "Accepted manifest must preserve attempt");
                } catch (const std::exception&) {
                    accepted = false;
                }
                ci::require(accepted == entry.path().filename().string().starts_with("good-"),
                            "Unexpected validation: " + entry.path().string());
                ++checked;
            }
        }
        ci::require(checked >= 12, "All archive fixtures must execute");
        std::cout << checked << " artifact/manifest boundaries passed\n";
    } catch (const std::exception& failure) {
        result = 1;
        std::cerr << failure.what() << '\n';
    }
    return result;
}
