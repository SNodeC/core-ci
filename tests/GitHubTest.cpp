#include "GitHub.h"

#include <core/SNodeC.h>
#include <cstdlib>
#include <exception>
#include <iostream>
#include <utils/Config.h>

int main(int argc, char* argv[]) {
    auto* config = utils::Config::configRoot.newSubCommand<coreci::GitHubConfig>();
    coreci::GitHub github(*config);

    core::SNodeC::init(argc, argv);

    int calls = 0;
    int status = -1;
    bool submitted = false;
    const char* expectation = std::getenv("CORE_CI_TEST_EXPECTED_STATUS");
    const int expected = expectation == nullptr ? 200 : std::atoi(expectation);
    const char* requestedRepository = std::getenv("CORE_CI_TEST_REPOSITORY");
    const std::string repository = requestedRepository == nullptr ? "Owner/example" : requestedRepository;
    try {
        github.loadCredentials();
        const auto completed = [&](const coreci::GitHubResult& result) {
            ++calls;
            status = result.status;
            submitted = result.submitted;
            if (result.status == 200 && result.body.value("full_name", "") != repository) {
                status = -2;
            }
            if (requestedRepository != nullptr && result.status == 200 && result.error.empty()) {
                std::cout << nlohmann::json{{"repository", result.body.value("full_name", "")},
                                            {"id", result.body.value("id", std::uint64_t{0})}}
                                 .dump()
                          << '\n';
            }
        };
        if (std::getenv("CORE_CI_TEST_PUBLIC_READ") != nullptr) {
            github.publicRead(repository, "", completed);
        } else {
            const char* method = std::getenv("CORE_CI_TEST_METHOD");
            github.request(repository, method == nullptr ? "GET" : method, "", nullptr, {{"contents", "read"}}, completed);
        }
    } catch (const std::exception& error) {
        std::cerr << error.what() << '\n';
    }

    const int loopStatus = core::SNodeC::start();
    const char* submittedSetting = std::getenv("CORE_CI_TEST_EXPECTED_SUBMISSION");
    const bool expectedSubmission = submittedSetting == nullptr || std::string(submittedSetting) == "1";
    if (submitted != expectedSubmission) {
        status = -3;
    }
    if (calls != 1 || status != expected) {
        std::cerr << "callback count=" << calls << " status=" << status << " expected=" << expected << '\n';
    }
    return calls == 1 && status == expected ? loopStatus : 1;
}
