// The town map picture without its painted place markers and TOWN MAP
// title, for the port's widescreen town map, which draws them as vector art.
// The picture comes from the player's disc, so it is made here rather than
// shipped.
#pragma once

#include "binary.hpp"

#include <filesystem>

namespace phantomcrash::extract {

// The map's picture: the top-left 640x416 of town_map_tex 0.
inline constexpr int town_map_width = 640;
inline constexpr int town_map_height = 416;
// Where the scrubbed picture is written, under the converted assets.
inline constexpr const char *scrubbed_town_map = "textures/scroll/town_map_tex/0_scrubbed.png";

// The map's 640x416 of `rgba` (the `width` x `height` atlas, RGBA) with the
// markers and the title filled in from the picture round them, patch by
// patch (exemplar inpainting, Criminisi et al. 2004): RGBA, 640x416.
[[nodiscard]] Bytes scrub_town_map(int width, int height, ByteView rgba);

// Writes the scrubbed map under `output` (the converted assets) from the
// texture pack in the extracted disc's `media`.
void write_scrubbed_town_map(const std::filesystem::path &media,
                             const std::filesystem::path &output);

} // namespace phantomcrash::extract
