#include "adapters/github/HttpClient.h"

#include <core/SNodeC.h>
#include <iostream>
int main(int argc, char** argv) {
    int result = 1;
    if (argc == 4) {
        const auto base = coreci::github::httpsUrl(argv[1]);
        if (base) {
            core::SNodeC::init(1, argv);
            coreci::github::HttpClient client(*base, argv[2]);
            const coreci::DispatchBuild b{"probe.t.1.0.0.r1",
                                          1,
                                          "probe",
                                          {"owner", "probe"},
                                          "v1.0.0",
                                          *coreci::Version::parse("1.0.0"),
                                          1,
                                          "t",
                                          {},
                                          {"owner", "recipes", "build.yml", "trunk"}};
            client.request(coreci::github::dispatch(b, "https://localhost", "smoke-token"), [&](const auto& response) {
                std::cout << "HTTP status " << response.status << '\n';
                result = response.status == std::stoi(argv[3]) ? 0 : 1;
            });
            const int loop = core::SNodeC::start();
            if (loop != 0) {
                result = 1;
            }
        }
    }
    return result;
}
