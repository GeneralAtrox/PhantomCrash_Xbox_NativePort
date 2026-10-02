#pragma once
#include "desktop_dynamic_lights.hpp"

namespace phantomcrash {
// Optional port-authored lighting, overlaid by mods/data/world/dynamic_lighting.json.
[[nodiscard]] std::vector<WorldLight> light_sources(const DataRoots &roots, const char *group,
                                                    int stage);
} // namespace phantomcrash
