#ifndef CORE_CI_UI_CONFIG_H
#define CORE_CI_UI_CONFIG_H

#include <string>
#include <string_view>
#include <utils/SubCommand.h>

namespace coreci {
    class UiConfig : public utils::SubCommand {
    public:
        constexpr static std::string_view NAME{"ui"};
        constexpr static std::string_view DESCRIPTION{"Configuration-writing UI origin"};
        explicit UiConfig(utils::SubCommand* parent);
        std::string origin;
    };
} // namespace coreci

#endif
