#include "termchart/plugin/manifest.hpp"

namespace termchart::plugin {

bool manifest_valid(const Manifest& manifest) noexcept {
    return !manifest.name.empty();
}

std::string describe(const Manifest& manifest) {
    std::string out = manifest.name;
    out += '@';
    out += manifest.version;
    if (!manifest.description.empty()) {
        out += " — ";
        out += manifest.description;
    }
    return out;
}

}  // namespace termchart::plugin
