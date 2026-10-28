#pragma once

#include "Database.h"
#include "Graph.h"

namespace coreci {
    class GraphStore {
    public:
        using Callback = std::function<void(const DatabaseResult&, const Json&)>;
        explicit GraphStore(Database& database)
            : database(database) {
        }
        void restore(const Database::Callback& callback);
        void change(const std::function<Json(Json&)>& apply, const Callback& callback);
        const Json& graph() const {
            return state;
        }
        bool mutationsAllowed() const {
            return database.mutationsAllowed();
        }
        Json view() const;
        Json publicView() const;
        std::function<void(bool, bool)> onChanged;

    private:
        Json view(const Json& state) const;
        Json publicView(const Json& state) const;
        Database& database;
        Json state = graph::empty();
    };
} // namespace coreci
