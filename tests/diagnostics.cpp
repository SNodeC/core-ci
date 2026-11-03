#include "fixtures.h"

#include <iostream>
#include <openssl/pem.h>

namespace {
    std::string response;
    std::string expectedMethod;
    int status = 200;
    unsigned int calls = 0;
    ci::Json responses;
    std::vector<std::string> requests;
    const std::string sentinel = "private-response-sentinel";
} // namespace

// Test-only linker redirection at the public HTTPS boundary. All JSON parsing
// and Controller token/api/checked behavior below is the actual production code.
extern "C" void wrappedHttps(const std::string&, const std::string&, const std::string&, const std::string&, const ci::HttpDone&) asm(
    "__wrap__ZN2ci5httpsERKNSt7__cxx1112basic_stringIcSt11char_traitsIcESaIcEEES7_S7_S7_RKSt8functionIFviS7_S7_S7_EE");

extern "C" void
wrappedHttps(const std::string& url, const std::string& method, const std::string&, const std::string& access, const ci::HttpDone& done) {
    ci::require(url.starts_with("https://api.github.com/") && method == expectedMethod && !access.empty(), "Expected public boundary");
    ++calls;
    requests.push_back(url);
    const std::string body = responses.is_object() ? responses.at(url).dump() : response;
    done(status, body, "", method + " https://api.github.com/fixture response: HTTP " + std::to_string(status));
}

int main() {
    int result = 0;
    try {
        std::unique_ptr<EVP_PKEY, decltype(&EVP_PKEY_free)> key(EVP_PKEY_Q_keygen(nullptr, nullptr, "RSA", 2048), EVP_PKEY_free);
        std::unique_ptr<BIO, decltype(&BIO_free)> buffer(BIO_new(BIO_s_mem()), BIO_free);
        ci::require(key && buffer && PEM_write_bio_PrivateKey(buffer.get(), key.get(), nullptr, nullptr, 0, nullptr, nullptr) == 1,
                    "Disposable signing fixture");
        char* bytes = nullptr;
        const auto size = BIO_get_mem_data(buffer.get(), &bytes);
        ci::Store store;
        ci::Controller controller(store, fixture::deployment(), "17", "19", std::string(bytes, size));
        std::string error;
        ci::Json value;
        const ci::Value checked = [&](const ci::Json& received, const std::string& failure) {
            value = received;
            error = failure;
        };
        ci::Controller invalidKey(store, fixture::deployment(), "17", "19", "");
        calls = 0;
        invalidKey.token([&](const std::string& token, const std::string& failure) {
            ci::require(token.empty(), "Signing failure returns no token");
            error = failure;
        });
        ci::require(calls == 0 && error == "GitHub App identity/key missing or oversized",
                    "Synchronous token signing failure reaches callback safely");
        for (const std::string body : {"{\"token\":\"" + sentinel, "{\"token\":[\"" + sentinel + "\"]}"}) {
            expectedMethod = "POST";
            response = body;
            status = 201;
            calls = 0;
            controller.token([&](const std::string& token, const std::string& failure) {
                ci::require(token.empty(), "A malformed token cannot escape");
                error = failure;
            });
            ci::require(calls == 1 && error.starts_with("Invalid JSON: ") && error.find(sentinel) == std::string::npos,
                        "Installation-token JSON failures are controller-authored and token POST is not retried");
            std::cout << error << '\n';
        }
        expectedMethod = "GET";
        response = "{\"private\":\"" + sentinel;
        status = 200;
        calls = 0;
        controller.api("fixture-access", "/fixture?secret", checked);
        ci::require(calls == 1 && error.starts_with("Invalid JSON: ") && error.find(sentinel) == std::string::npos && value.is_null(),
                    "Malformed API JSON never exposes a remote excerpt");
        std::cout << error << '\n';
        ci::checked(checked, [](const ci::Json&) {
            const auto parsed = ci::Json::parse("{\"receipt\":\"" + sentinel);
            ci::require(parsed.is_object(), "Receipt object expected");
        })({}, "");
        ci::require(error.starts_with("Invalid JSON: ") && error.find(sentinel) == std::string::npos, "Receipt JSON is classified safely");
        std::cout << error << '\n';
        status = 403;
        response = sentinel;
        controller.api("fixture-access", "/fixture?secret", checked);
        ci::require(error == "GET https://api.github.com/fixture response: HTTP 403", "Semantic HTTP error preserves safe context only");
        std::cout << "Token, API and checked JSON secrecy passed\n";
        status = 200;
        const std::string repository = "Fixture/worker";
        const std::string application(40, '1');
        const std::string ciRevision(40, '2');
        const std::string workflowRevision(40, '3');
        const std::string workflow = "approved workflow fixture\n";
        const auto workflowBlob = ci::hash("blob " + std::to_string(workflow.size()) + std::string(1, '\0') + workflow, "SHA1");
        auto profile = fixture::deployment();
        profile["source_workflow"]["blob_sha"] = workflowBlob;
        ci::Controller selectedController(store, profile, "17", "19", std::string(bytes, size));
        ci::Json definition = {{"schema", 1},
                               {"package", "worker"},
                               {"title", "Fixture"},
                               {"matrix", ci::Json::array({fixture::target()})},
                               {"depends_on", ci::Json::array()},
                               {"build", {{"native", "ci/build.sh"}}}};
        ci::Json enrollment = {
            {repository, {{"ref", "reviewed"}, {"commit", ciRevision}, {"blob", std::string(40, '4')}, {"definition", definition}}}};
        const auto origin = "https://api.github.com/repos/" + repository;
        responses = {{origin, {{"id", 42}, {"full_name", repository}}},
                     {origin + "/commits/" + ci::encode("refs/tags/v1.0.0"), {{"sha", application}}},
                     {origin + "/commits/reviewed", {{"sha", workflowRevision}}},
                     {origin + "/contents/.github/workflows/build.yml?ref=" + workflowRevision,
                      {{"type", "file"}, {"encoding", "base64"}, {"content", ci::base64(workflow)}, {"sha", workflowBlob}}}};
        auto selection = std::make_shared<ci::Selection>();
        selection->done = checked;
        requests.clear();
        selectedController.resolve("fixture-access", enrollment, repository, "v1.0.0", selection);
        ci::require(error.empty() && requests.size() == 4 && selection->pending == 0,
                    "Selection reads tag and approved workflow, never definition or hooks from the application tag");
        const auto original = value;
        const auto& source = original.at(repository).at("source");
        ci::require(source.at("commit") == application && source.at("definition_revision") == ciRevision &&
                        source.at("definition_blob_sha") == enrollment.at(repository).at("blob") &&
                        original.at(repository).at("definition") == definition,
                    "Existing tag without CI freezes the enrolled definition and its independent exact revision");
        auto graph = fixture::graph();
        ci::selectRequest(graph, original, repository);
        const auto history = graph.at("executions").at(0);
        enrollment[repository]["commit"] = std::string(40, '5');
        selection = std::make_shared<ci::Selection>();
        selection->done = checked;
        selectedController.resolve("fixture-access", enrollment, repository, "v1.0.0", selection);
        ci::require(error.empty() && value.at(repository).at("source").at("commit") == application &&
                        value.at(repository).at("source").at("definition_revision") == std::string(40, '5'),
                    "CI refresh changes only the next selection's frozen CI revision");
        ci::selectRequest(graph, value, repository);
        ci::require(graph.at("executions").size() == 2 && graph.at("executions").at(0) == history,
                    "New CI revision creates new immutable work without rewriting assigned history");
        std::cout << "Independent source, CI and workflow revisions passed\n";
        profile["repositories"]["Fixture/client"] = 44;
        ci::Controller recursive(store, profile, "17", "19", std::string(bytes, size));
        definition["package"] = "client";
        definition["depends_on"] = ci::Json::array({"worker"});
        enrollment["Fixture/client"] = {
            {"ref", "reviewed"}, {"commit", std::string(40, '6')}, {"blob", std::string(40, '7')}, {"definition", definition}};
        const std::string clientOrigin = "https://api.github.com/repos/Fixture/client";
        responses[clientOrigin] = {{"id", 44}, {"full_name", "Fixture/client"}};
        responses[clientOrigin + "/commits/" + ci::encode("refs/tags/v2.0.0")] = {{"sha", std::string(40, '8')}};
        responses[clientOrigin + "/commits/reviewed"] = {{"sha", workflowRevision}};
        responses[clientOrigin + "/contents/.github/workflows/build.yml?ref=" + workflowRevision] = {
            {"type", "file"}, {"encoding", "base64"}, {"content", ci::base64(workflow)}, {"sha", workflowBlob}};
        responses[origin + "/tags?per_page=100&page=1"] = ci::Json::array({{{"name", "v1.0.0"}}});
        selection = std::make_shared<ci::Selection>();
        selection->done = checked;
        requests.clear();
        recursive.resolve("fixture-access", enrollment, "Fixture/client", "v2.0.0", selection);
        ci::require(error.empty() && value.size() == 2 && requests.size() == 9 && selection->pending == 0 &&
                        value.at(repository).at("source").at("tag") == "v1.0.0" &&
                        value.at(repository).at("source").at("definition_revision") == std::string(40, '5') &&
                        value.at("Fixture/client").at("source").at("definition_revision") == std::string(40, '6'),
                    "Recursive prerequisite selection uses each enrolled CI revision and normal highest-tag discovery");
        ci::require(ci::selectedTag(graph, repository, "v1.0.0", application),
                    "Duplicate authenticated source events remain idempotent after CI refresh");

    } catch (const std::exception& failure) {
        std::cerr << failure.what() << '\n';
        result = 1;
    }
    return result;
}
