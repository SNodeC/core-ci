#include "Fixture.h"
#include "adapters/mariadb/MariaDBStateStore.h"

using namespace coreci;
using namespace coreci::persistence;
int main() {
    History h({0, 100, 3, 0});
    h.load(10);
    const auto encoded = encodeState(h.engine.stored());
    CHECK(decodeState(encoded) == h.engine.stored());
    for (const auto& table : stateTables()) {
        CHECK(selectState(table).find("coreci_log") == std::string::npos);
        for (const auto& source : encoded.at(table.name)) {
            std::vector<Field> fields;
            for (std::size_t index = 0; index < source.size(); ++index) {
                if (source[index] && !table.columns[index].numeric) {
                    const auto literal = sqlText(*source[index]);
                    fields.push_back(literal.substr(11, literal.size() - 12));
                } else {
                    fields.push_back(source[index]);
                }
            }
            CHECK(decodeFields(table, fields) == source);
            for (std::size_t index = 0; index < fields.size(); ++index) {
                auto invalid = fields;
                invalid[index] = table.columns[index].numeric ? "1x" : "0G";
                bool rejected = false;
                try {
                    decodeFields(table, invalid);
                } catch (const std::invalid_argument&) {
                    rejected = true;
                }
                CHECK(rejected);
            }
        }
    }
    const Table text{"probe", 1, {{"text", false}}};
    const Table numeric{"probe", 1, {{"number", true}}};
    CHECK_EQ(*decodeFields(numeric, {"18446744073709551615"})[0], "18446744073709551615");
    CHECK_EQ(*decodeFields(numeric, {"-9223372036854775808"})[0], "-9223372036854775808");
    CHECK_EQ(*decodeFields(text, {""})[0], "");
    CHECK_EQ(*decodeFields(text, {"c3a9f09f9880"})[0], "é😀");
    for (const auto& bad : {"x", "a", "0G", "610062"}) {
        bool rejected = false;
        try {
            decodeFields(text, {bad});
        } catch (const std::invalid_argument&) {
            rejected = true;
        }
        CHECK(rejected);
    }
    const auto statements = stateSql({}, h.engine.stored(), "quote '\\ with UTF-8 é");
    CHECK(statements.back().find("quote") == std::string::npos);
    CHECK(statements.back().find("coreci_log") != std::string::npos);
    mariadb::MariaDBStateStore unopened({});
    bool unavailable = false, commitFailed = false;
    unopened.load(
        [](auto) {
            CHECK(false);
        },
        [&](const auto& error) {
            unavailable = error.kind == StoreError::Kind::Unavailable;
        });
    unopened.commit(
        {},
        {},
        "",
        [] {
            CHECK(false);
        },
        [&](const auto& error) {
            commitFailed = error.kind == StoreError::Kind::Unavailable;
        });
    CHECK(unavailable);
    CHECK(commitFailed);
    return checkResult();
}
