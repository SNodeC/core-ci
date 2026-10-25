#include "UiConfig.h"

namespace coreci {
    UiConfig::UiConfig(utils::SubCommand* parent)
        : SubCommand(parent, this, "UI") {
        setConfigurable(addOptionVariable("--origin",
                                          origin,
                                          "Exact allowed browser origin, including scheme and port",
                                          "origin",
                                          std::string("http://127.0.0.1:8080"),
                                          CLI::TypeValidator<std::string>()),
                        true);
    }
} // namespace coreci
