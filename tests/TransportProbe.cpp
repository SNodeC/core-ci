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
            const coreci::DispatchBuild b{"probe~t~1.0.0~r1",
                                          1,
                                          "probe",
                                          {"owner", "probe"},
                                          "v1.0.0",
                                          *coreci::Version::parse("1.0.0"),
                                          1,
                                          "t",
                                          {},
                                          {"owner", "recipes", "build.yml", "trunk"}};
            client.request(
                coreci::github::dispatchPath(b),
                "smoke-token",
                [&](int status, std::string, std::string) {
                    std::cout << "HTTP status " << status << '\n';
                    result = status == std::stoi(argv[3]) ? 0 : 1;
                    if (result == 0 && status == 204) {
                        // Exercise the declaration fixture's original token/header expectations over real HTTP.
                        client.request("/repos/o/registry", "token", [&](int status, std::string, std::string) {
                            result = status == 200 ? 0 : 1;
                            if (result == 0) {
                                client.request(
                                    "/repos/o/registry/contents/ci%2Fregistry.json?ref=master",
                                    "token",
                                    [&](int status, std::string, std::string) {
                                        result = status == 200 ? 0 : 1;
                                    },
                                    {},
                                    true);
                            }
                        });
                    }
                },
                coreci::github::dispatchBody(b, "https://localhost"));
            const int loop = core::SNodeC::start();
            if (loop != 0) {
                result = 1;
            }
        }
    }
    return result;
}
