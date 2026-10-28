#include "GitHub.h"

#include "HttpClient.h"

#include <core/SNodeC.h>
#include <csignal>
#include <cstdlib>
#include <exception>
#include <iostream>
#include <utils/Config.h>

int main(int argc, char* argv[]) {
    auto* config = utils::Config::configRoot.newSubCommand<coreci::GitHubConfig>();
    coreci::GitHub github(*config);
    std::unique_ptr<coreci::HttpClient> disabled;
    if (std::getenv("CORE_CI_TEST_DIRECT_DISABLED") != nullptr) {
        disabled = std::make_unique<coreci::HttpClient>("probe-http", "api.github.com");
    }

    core::SNodeC::init(argc, argv);

    int calls = 0;
    const bool reuse = std::getenv("CORE_CI_TEST_REUSE") != nullptr;
    const bool parallel = std::getenv("CORE_CI_TEST_PARALLEL") != nullptr;
    const bool shutdown = std::getenv("CORE_CI_TEST_SHUTDOWN") != nullptr;
    int unsubmitted = 0;
    int uncertain = 0;
    const int expectedCalls = parallel ? 12 : (reuse ? 4 : 1);
    bool correlated = true;
    bool returned = false;
    std::function<void(const coreci::GitHubResult&)> completed;
    int status = -1;
    bool submitted = false;
    const char* expectation = std::getenv("CORE_CI_TEST_EXPECTED_STATUS");
    const int expected = expectation == nullptr ? 200 : std::atoi(expectation);
    const char* requestedRepository = std::getenv("CORE_CI_TEST_REPOSITORY");
    const std::string repository = requestedRepository == nullptr ? "Owner/example" : requestedRepository;
    try {
        github.loadCredentials();
        completed = [&](const coreci::GitHubResult& result) {
            ++calls;
            if (const char* error = std::getenv("CORE_CI_TEST_EXPECTED_ERROR")) {
                correlated = correlated && returned && result.error.find(error) != std::string::npos;
            }
            if (shutdown) {
                unsubmitted += !result.submitted && result.error == "HTTPS request was not submitted during shutdown";
                uncertain += result.submitted && result.status == 0 && result.error == "HTTPS response has an invalid status";
            }
            status = result.status;
            submitted = result.submitted;
            if (result.status == 200 && result.body.value("full_name", "") != repository) {
                status = -2;
            }
            if (reuse && calls == 2) {
                github.request(repository, "GET", "", nullptr, {{"contents", "read"}}, completed);
                github.request(repository, "GET", "", nullptr, {{"actions", "read"}}, completed);
            }
            if (requestedRepository != nullptr && result.status == 200 && result.error.empty()) {
                std::cout << nlohmann::json{{"repository", result.body.value("full_name", "")},
                                            {"id", result.body.value("id", std::uint64_t{0})}}
                                 .dump()
                          << '\n';
            }
        };
        if (disabled) {
            disabled->request("GET", "/probe", {}, "", [&](const coreci::HttpResult& result) {
                completed({result.status, nullptr, result.error, result.submitted});
            });
        } else if (std::getenv("CORE_CI_TEST_PUBLIC_READ") != nullptr) {
            github.request(repository, "GET", "", nullptr, {{"contents", "read"}}, completed, true);
        } else if (parallel) {
            for (int index = 0; index < expectedCalls; ++index) {
                github.request(repository,
                               "GET",
                               "/probe/" + std::to_string(index),
                               nullptr,
                               {{"contents", "read"}},
                               [&, index](const coreci::GitHubResult& result) {
                                   correlated = correlated && (shutdown || result.body.value("probe", -1) == index);
                                   completed(result);
                               });
            }
        } else if (reuse) {
            github.request(repository, "GET", "", nullptr, {{"contents", "read"}}, completed);
            github.request(repository, "GET", "", nullptr, {{"contents", "read"}}, completed);
        } else {
            const char* method = std::getenv("CORE_CI_TEST_METHOD");
            github.request(repository, method == nullptr ? "GET" : method, "", nullptr, {{"contents", "read"}}, completed);
        }
        returned = true;
    } catch (const std::exception& error) {
        std::cerr << error.what() << '\n';
    }

    const int loopStatus = core::SNodeC::start();
    const char* submittedSetting = std::getenv("CORE_CI_TEST_EXPECTED_SUBMISSION");
    const bool expectedSubmission = submittedSetting == nullptr || std::string(submittedSetting) == "1";
    if ((!shutdown && submitted != expectedSubmission) || !correlated || (shutdown && (unsubmitted != 11 || uncertain != 1))) {
        status = -3;
    }
    if (calls != expectedCalls || status != expected) {
        std::cerr << "callback count=" << calls << " status=" << status << " expected=" << expected << " queued-unsubmitted=" << unsubmitted
                  << " submitted-unknown=" << uncertain << '\n';
    }
    return calls == expectedCalls && status == expected ? (shutdown && loopStatus == -SIGTERM ? 0 : loopStatus) : 1;
}
