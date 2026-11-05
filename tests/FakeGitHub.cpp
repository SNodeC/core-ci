#include <core/SNodeC.h>
#include <express/tls/in/WebApp.h>
#include <fstream>
#include <iostream>
#include <set>
#include <utils/Config.h>
#include <vector>
int main(int argc, char** argv) {
    express::tls::in::WebApp server("fake");
    server.getConfig()->Local::setPort(9444);
    auto& root = utils::Config::configRoot;
    auto* log = root.addOption("--requests", "request trace", "file", std::string{}, CLI::Validator());
    auto* cert = root.addOption("--certificate", "certificate", "file", std::string{}, CLI::Validator());
    auto* key = root.addOption("--private-key", "key", "file", std::string{}, CLI::Validator());
    auto* port = root.addOption("--port", "port", "port", std::string{"9444"}, CLI::Validator());
    core::SNodeC::init(argc, argv);
    server.getConfig()->setCert(cert->as<std::string>());
    server.getConfig()->setCertKey(key->as<std::string>());
    server.getConfig()->Local::setHost("127.0.0.1")->setPort(port->as<std::uint16_t>());
    server.getConfig()->setReuseAddress();
    std::size_t dispatches = 0;
    bool tagPages = false;
    std::set<std::pair<std::string, std::string>> holds;
    std::vector<std::shared_ptr<express::Response>> held;
    std::vector<std::pair<std::size_t, std::shared_ptr<express::Response>>> waiters;
    server.use([&](const auto& req, const auto& res) {
        if (req->path == "/tag-pages") {
            tagPages = true;
            res->send("enabled");
        } else if (req->path == "/hold") {
            holds.emplace(req->query("id"), req->query("kind"));
            res->send("held");
        } else if (req->path == "/wait") {
            const auto wanted = static_cast<std::size_t>(std::stoul(req->query("count")));
            if (dispatches >= wanted) {
                res->send("ready");
            } else {
                waiters.emplace_back(wanted, res);
            }
        } else {
            std::ofstream out(log->as<std::string>(), std::ios::app);
            const std::string body(req->body.begin(), req->body.end());
            out << req->method << ' ' << req->url << ' ' << body << '\n';
            out.flush();
            if (req->get("Authorization") != "Bearer smoke-token" || req->get("Accept") != "application/vnd.github+json" ||
                req->get("X-GitHub-Api-Version") != "2022-11-28") {
                out << "BAD HEADERS\n";
                res->status(400).send("bad headers");
            } else if (req->method == "GET") {
                const auto tags = [](const std::vector<std::string>& names) {
                    std::string body = "[";
                    for (const auto& name : names) {
                        if (body.size() > 1) {
                            body += ',';
                        }
                        body += "{\"name\":\"" + name + "\",\"commit\":{\"sha\":\"" + std::string(40, 'a') + "\"}}";
                    }
                    return body + "]";
                };
                std::string body = "[]";
                if (tagPages) {
                    if (req->path == "/repos/owner/a/tags" || req->path == "/repos/owner/b/tags") {
                        if (req->query("page") == "2") {
                            body = req->path == "/repos/owner/b/tags" ? "malformed" : tags({"v1.0.0", "1.0.0", "v9.0.0-rc.1"});
                        } else {
                            body = tags({"v0.9.0"});
                            res->set("Link",
                                     "<https://localhost:" + std::to_string(port->as<std::uint16_t>()) + req->path +
                                         "?page=2>; rel=\"next\"");
                        }
                    } else if (req->path == "/repos/owner/c/tags") {
                        body = tags({"v8.0.0"});
                        res->set("Link", "<https://off-origin.invalid/tags?page=2>; rel=\"next\"");
                    } else if (req->path == "/repos/owner/d/tags") {
                        body = tags({"v2.0.0"});
                    } else {
                        body = tags({"v9.0.0-rc.1", "release-invalid"});
                    }
                }
                res->set("Content-Type", "application/json").send(body);
            } else if (req->method == "POST" && req->path.ends_with("/dispatches") && req->get("Content-Type") == "application/json") {
                ++dispatches;
                auto found = holds.end();
                for (auto i = holds.begin(); i != holds.end(); ++i) {
                    if (body.find("\"build_id\":\"" + i->first + "\"") != std::string::npos &&
                        req->path.find(i->second + ".yml") != std::string::npos) {
                        found = i;
                    }
                }
                if (found != holds.end()) {
                    holds.erase(found);
                    held.push_back(res);
                } else {
                    res->status(204).end();
                }
                std::erase_if(waiters, [&](const auto& waiter) {
                    const bool ready = dispatches >= waiter.first;
                    if (ready) {
                        waiter.second->send("ready");
                    }
                    return ready;
                });
            } else {
                res->status(404).end();
            }
        }
    });
    server.listen([](const auto&, const auto& state) {
        if (state != core::socket::State::OK) {
            std::cerr << "fake listener failed\n";
            core::SNodeC::stop();
        }
    });
    return core::SNodeC::start();
}
