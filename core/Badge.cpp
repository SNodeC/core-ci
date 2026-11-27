#include "Badge.h"

#include <algorithm>
#include <string_view>

namespace coreci {
    namespace {
        std::string xml(const std::string& text) {
            std::string result;
            static const std::string_view entities[] = {"&amp;", "&lt;", "&gt;", "&quot;", "&apos;"};
            for (const char c : text) {
                const auto at = std::string_view("&<>\"'").find(c);
                result += at == std::string_view::npos ? std::string(1, c) : std::string(entities[at]);
            }
            return result;
        }
        std::size_t width(const std::string& text) {
            return 6 * std::count_if(text.begin(),
                                     text.end(),
                                     [](unsigned char c) {
                                         return (c & 0xc0) != 0x80;
                                     }) +
                   10;
        }
    } // namespace
    std::string badge(const std::string& label, const std::string& message, const std::string& colour) {
        const auto left = width(label), right = width(message);
        return "<svg xmlns=\"http://www.w3.org/2000/svg\" width=\"" + std::to_string(left + right) +
               "\" height=\"20\" role=\"img\"><title>" + xml(label) + ": " + xml(message) + "</title><rect width=\"" +
               std::to_string(left) + "\" height=\"20\" fill=\"#555\"/><rect x=\"" + std::to_string(left) + "\" width=\"" +
               std::to_string(right) + "\" height=\"20\" fill=\"" + xml(colour) +
               "\"/><g fill=\"white\" font-family=\"Verdana,DejaVu Sans,sans-serif\" font-size=\"11\" text-anchor=\"middle\"><text x=\"" +
               std::to_string(left / 2) + "\" y=\"14\">" + xml(label) + "</text><text x=\"" + std::to_string(left + right / 2) +
               "\" y=\"14\">" + xml(message) + "</text></g></svg>";
    }
    std::string unitBadge(const WaveController& engine, const Unit& unit) {
        std::string message = "unknown", colour = "#6e7781";
        if (engine.pipeline().packages().contains(unit.package) &&
            engine.pipeline().packages().at(unit.package).targets.contains(unit.target)) {
            const auto& state = engine.units().at(unit);
            if (!engine.releases().at(unit.package)) {
                message = "no release";
            } else if (state.state == State::Published) {
                message = state.published->version.text() + "-" + std::to_string(state.published->revision);
                colour = "#2ea44f";
            } else if (state.state == State::Blocked) {
                message = "blocked";
                colour = "#bf8700";
            } else {
                const bool failed = state.state == State::Failed;
                message = std::string(failed ? "failed " : "building ") + engine.releases().at(unit.package)->version.text();
                colour = failed ? "#cf222e" : "#0969da";
            }
        }
        return badge(unit.package, message, colour);
    }
} // namespace coreci
