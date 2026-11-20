#pragma once
#include "adapters/github/Declarations.h"
#include "persistence/StateSession.h"

#include <express/Router.h>

namespace coreci::app {
    express::Router apiRouter(persistence::StateSession& state, const std::string& secret);
    express::Router webhookRouter(persistence::StateSession& state,
                                  const github::DeclarationFile& registry,
                                  const std::string& secret,
                                  std::function<void(std::function<void(bool)>)> reload);
} // namespace coreci::app
