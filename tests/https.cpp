#include "Controller.h"

#include <core/EventReceiver.h>
#include <core/SNodeC.h>
#include <iostream>
#include <utils/Config.h>

int main(int argc, char* argv[]) {
    int result = 1;
    core::SNodeC::init(argc, argv);
    core::EventReceiver::atNextTick([&]() {
        try {
            ci::https(std::getenv("CI_TEST_URL"),
                      "POST",
                      "fixture-body",
                      "fixture-token",
                      [&](int status, const std::string& body, const std::string& location) {
                          const int expected = std::stoi(std::getenv("CI_TEST_STATUS"));
                          if (status == expected && (status != 200 || body == "fixture-response") &&
                              (status != 302 || location == "https://unrelated.invalid/never-follow")) {
                              result = 0;
                          }
                          if (result != 0) {
                              std::cerr << "HTTP status " << status << ", expected " << expected << "\n";
                          }
                          core::SNodeC::stop();
                      });
        } catch (const std::exception& failure) {
            std::cerr << failure.what() << '\n';
            core::SNodeC::stop();
        }
    });
    core::SNodeC::start();
    return result;
}
