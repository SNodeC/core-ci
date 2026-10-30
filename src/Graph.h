#pragma once

#include "Crypto.h"

#include <string>

namespace coreci {
    namespace graph {
        template <typename State>
        auto& work(State& state, const std::string& id) {
            return state.at(state.at("executions").contains(id) ? "executions" : "retention").at(id);
        }
        std::vector<std::string> workIds(const Json& state);
        Json empty();
        Json coordinates(const Json& target);
        std::vector<std::string> validateSources(const Json& sources, const Json& executions = Json::object());
        Json select(Json& state, const std::string& root, const Json& sources, bool explicitRequest, std::uint64_t since);
        Json advance(Json& state);
        Json retain(Json& state, const Json& deployment);
        bool retirable(const Json& node);
        void retire(Json& state, const std::string& id, const std::string& attempt);
        Json retry(Json& state, const std::string& id);
        void observe(Json& node, const std::string& attempt, bool publication, const Json& run, const Json& facts);
        void built(Json& node, const std::string& attempt, const Json& result);
        void published(Json& node, const std::string& attempt, const Json& receipt);
        void failed(Json& node, const std::string& attempt, const std::string& reason);
        Json inputs(const Json& node, bool publication);
        bool eligible(const Json& state, const Json& node, bool pending);
        Json blockers(const Json& state, const Json& node);
    } // namespace graph
} // namespace coreci
