#include "app/Config.h"
#include "legacy/Engine.h"
#include "legacy/MariaDBEventStore.h"
#include "persistence/StateSql.h"

#include <core/SNodeC.h>
#include <iostream>
#include <utils/Config.h>

namespace {
    class Section : public utils::SubCommand {
    public:
        static constexpr std::string_view NAME = "coreci", DESCRIPTION = "One-shot core-ci migration";
        explicit Section(utils::SubCommand* parent)
            : SubCommand(parent, this, "Applications")
            , options(*this) {
        }
        coreci::app::Config options;
    };
} // namespace
int main(int argc, char** argv) {
    auto& config = utils::Config::configRoot.newSubCommand<Section>()->options;
    core::SNodeC::init(argc, argv);
    bool valid = true, migrated = false;
    database::mariadb::MariaDBConnectionDetails details;
    details.connectionName = "coreci-migrate";
    details.hostname = config.get("db-host");
    details.port = static_cast<unsigned>(config.number("db-port", 0, 65535, valid));
    details.socket = config.get("db-socket").empty() ? MYSQL_UNIX_ADDR : config.get("db-socket");
    details.username = config.get("db-user");
    details.database = config.get("db-name");
    details.password = config.secret("db-password-file", valid);
    const auto repository = config.get("workflow-repository");
    const auto slash = repository.find('/');
    if (slash == std::string::npos) {
        coreci::app::Config::invalid("workflow-repository", valid);
    }
    coreci::legacy::Engine engine({config.number("wave-delay", 0, 86400, valid),
                                   config.number("wave-deadline", 60, 86400, valid),
                                   static_cast<unsigned>(config.number("max-dispatch-attempts", 1, 20, valid)),
                                   config.number("retry-delay", 0, 86400, valid),
                                   config.get("feed-branch"),
                                   {repository.substr(0, slash), slash == std::string::npos ? "" : repository.substr(slash + 1), "main"},
                                   config.number("start-timeout", 1, 86400, valid)});
    std::uint64_t sequence = 0;
    std::vector<std::string> log;
    coreci::legacy::mariadb::MariaDBEventStore store(details);
    const auto error = [&](const auto&) {
        std::cerr << "migration failed; source history retained; inspect schema version before retrying\n";
        core::SNodeC::stop();
    };
    if (valid) {
        store.open(
            [&] {
                store.loadAll(
                    [&](auto seq, const coreci::legacy::Event& event) {
                        engine.handle(event, seq);
                        sequence = seq;
                        const auto row = std::get<coreci::legacy::persistence::EventRow>(coreci::legacy::persistence::encode(event, seq));
                        const auto description = row.eventType + " " + row.waveId.value_or("") + " " + row.buildId.value_or("") + " " +
                                                 row.reason.value_or("") + " " + row.conclusion.value_or("");
                        log.push_back("INSERT INTO coreci_log (seq,logged_at,description) VALUES (" + std::to_string(seq) + "," +
                                      std::to_string(*row.eventAt) + "," + coreci::persistence::sqlText(description) + ")");
                    },
                    [&] {
                        auto state = engine.convert();
                        state.sequence = sequence;
                        try {
                            const auto roundtrip = coreci::persistence::decodeState(coreci::persistence::encodeState(state));
                            if (roundtrip != state) {
                                throw std::invalid_argument("conversion lost state");
                            }
                            store.installState(
                                state,
                                log,
                                [&] {
                                    migrated = true;
                                    core::SNodeC::stop();
                                },
                                error);
                        } catch (const std::exception&) {
                            error(0);
                        }
                    },
                    error);
            },
            error);
        core::SNodeC::start();
    }
    std::cout << (migrated ? "migration complete\n" : "migration incomplete\n");
    return migrated ? 0 : 1;
}
