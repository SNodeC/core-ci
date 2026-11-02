#include "Controller.h"

#include <core/EventReceiver.h>
#include <core/SNodeC.h>
#include <core/timer/Timer.h>
#include <iostream>
#include <utils/Config.h>

int main(int argc, char* argv[]) {
    int result = 0;
    unsigned int completed = 0;
    std::vector<std::weak_ptr<bool>> lifetimes;
    std::unique_ptr<core::timer::Timer> teardownCheck;
    const unsigned int count = std::getenv("CI_TEST_SECOND_URL") == nullptr ? 1 : 2;
    core::SNodeC::init(argc, argv);
    core::EventReceiver::atNextTick([&]() {
        for (unsigned int index = 0; index < count; ++index) {
            try {
                auto delivered = std::make_shared<bool>(false);
                lifetimes.push_back(delivered);
                ci::https(index == 0 ? std::getenv("CI_TEST_URL") : std::getenv("CI_TEST_SECOND_URL"),
                          std::getenv("CI_TEST_METHOD"),
                          "fixture-body",
                          "fixture-token",
                          [&, delivered](int status, const std::string& body, const std::string& location, const std::string& context) {
                              const int expected = std::stoi(std::getenv("CI_TEST_STATUS"));
                              if (*delivered || status != expected || (status == 200 && body != "fixture-response") ||
                                  (status == 302 && location != "https://unrelated.invalid/never-follow")) {
                                  result = 1;
                              }
                              *delivered = true;
                              std::cout << "RESULT " << status << " " << context << '\n';
                              if (++completed == count) {
                                  if (std::getenv("CI_HTTPS_MEMCHECK")) {
                                      teardownCheck = std::make_unique<core::timer::Timer>(core::timer::Timer::singleshotTimer(
                                          [&]() {
                                              for (const auto& lifetime : lifetimes) {
                                                  if (!lifetime.expired()) {
                                                      result = 1;
                                                  }
                                              }
                                              std::cout << "Callback captures released before framework stop: " << (result == 0) << '\n';
                                              core::SNodeC::stop();
                                          },
                                          2));
                                  } else {
                                      core::SNodeC::stop();
                                  }
                              }
                          });
            } catch (const std::exception& failure) {
                result = 1;
                std::cerr << failure.what() << '\n';
                core::SNodeC::stop();
            }
        }
    });
    core::SNodeC::start();
    if (completed != count) {
        result = 1;
    }
    return result;
}
