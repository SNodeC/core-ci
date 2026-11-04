#include "Controller.h"

#include <core/EventReceiver.h>
#include <core/SNodeC.h>
#include <core/timer/Timer.h>
#include <dlfcn.h>
#include <iostream>
#include <netdb.h>
#include <utils/Config.h>

namespace {
    unsigned int reverseLookups = 0;
}

// Test-only interposition at libc: numeric endpoint formatting must not ask DNS.
extern "C" int
getnameinfo(const sockaddr* address, socklen_t size, char* host, socklen_t hostSize, char* service, socklen_t serviceSize, int flags) {
    using Native = int (*)(const sockaddr*, socklen_t, char*, socklen_t, char*, socklen_t, int);
    static const auto native = reinterpret_cast<Native>(dlsym(RTLD_NEXT, "getnameinfo"));
    int result = EAI_SYSTEM;
    if (std::getenv("CI_TEST_NUMERIC_PEERS") != nullptr && (flags & NI_NUMERICHOST) == 0) {
        ++reverseLookups;
        result = EAI_NONAME;
    } else if (native != nullptr) {
        result = native(address, size, host, hostSize, service, serviceSize, flags);
    }
    return result;
}

int main(int argc, char* argv[]) {
    int result = 0;
    unsigned int completed = 0;
    std::vector<std::weak_ptr<bool>> lifetimes;
    std::unique_ptr<core::timer::Timer> teardownCheck;
    const unsigned int count = std::getenv("CI_TEST_COUNT") != nullptr ? std::stoul(std::getenv("CI_TEST_COUNT"))
                                                                       : (std::getenv("CI_TEST_SECOND_URL") == nullptr ? 1 : 2);
    core::SNodeC::init(argc, argv);
    core::EventReceiver::atNextTick([&]() {
        for (unsigned int index = 0; index < count; ++index) {
            try {
                auto delivered = std::make_shared<bool>(false);
                lifetimes.push_back(delivered);
                ci::https(index == 0 || std::getenv("CI_TEST_SECOND_URL") == nullptr ? std::getenv("CI_TEST_URL")
                                                                                     : std::getenv("CI_TEST_SECOND_URL"),
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
    if (completed != count || reverseLookups != 0) {
        std::cerr << "Incomplete callbacks or unexpected reverse DNS lookups: " << reverseLookups << '\n';
        result = 1;
    }
    return result;
}
