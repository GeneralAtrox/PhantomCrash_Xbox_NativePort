#pragma once

#include "phantomcrash/json.hpp"

#include <array>
#include <cstdint>
#include <functional>
#include <map>
#include <string>
#include <string_view>
#include <vector>

namespace phantomcrash {

// Atlas art drawn in another colour: the data names an image as
// "<image>#<theme>" and data/ui/themes.json says what the theme changes.
// S.L.A.I.'s shops reuse Phantom Crash's shop screens this way, each in its
// own colour (a port addition).

// A region (atlas LTRB) of one image.
struct ImageThemeArea {
  std::string image;
  std::array<int, 4> region{};
};

// A picture fitted into a region of one image, centred, keeping its aspect
// and growing at most `max_scale` times (the shop's logo in place of the
// original's).
struct ImageThemePaste {
  std::string image;
  std::string picture;
  std::array<int, 4> rect{};   // in the picture
  std::array<int, 4> region{}; // in the image
  float max_scale{1.0F};
  bool clear{}; // the region is made transparent first
};

struct ImageTheme {
  // Source hues that take the theme's colour, as [from, to] degrees.
  std::vector<std::array<float, 2>> hues;
  float min_saturation{}; // 0-1; greyer texels keep their colour
  float hue{};            // degrees
  float saturation{1.0F}; // scales a matched texel's saturation
  float value{1.0F};      // scales a matched texel's brightness
  // Where to recolour; none: every themed image entirely.
  std::vector<ImageThemeArea> recolour;
  // Lettering or a logo painted into an image, cleared before recolouring:
  // each region is filled smoothly from the texels around it.
  std::vector<ImageThemeArea> erase;
  std::vector<ImageThemePaste> paste; // after recolouring (TextureCache::load_rgba)
};

using ImageThemes = std::map<std::string, ImageTheme, std::less<>>;

[[nodiscard]] ImageThemes parse_image_themes(const json::JsonValue &document);

struct ThemedImage {
  std::string_view image; // the file
  std::string_view theme; // empty for plain art
};

[[nodiscard]] ThemedImage split_themed_image(std::string_view key);

// One 0xRRGGBB colour recoloured as the theme recolours a texel of it (the
// lettering redrawn over themed art, data/ui/sprite_text.json).
[[nodiscard]] std::uint32_t themed_colour(const ImageTheme &theme, std::uint32_t rgb);

// Erases and recolours RGBA8 pixels loaded from `image` as `theme` says; the
// pastes are the loader's.
void apply_image_theme(const ImageTheme &theme, std::string_view image, std::uint8_t *pixels,
                       int width, int height, int pitch);

} // namespace phantomcrash
