#pragma once

#include "phantomcrash/json.hpp"

#include <array>
#include <cstdint>
#include <optional>
#include <string>
#include <vector>

namespace phantomcrash {

// Alpha animations of the original's 2D renderer (0xd391d), selected by draw
// flags. All share one 80-frame triangle wave, |2t - 1| over 81 frames at
// 60 Hz: "fade" is the wave itself (flag 0x20), "pulse" is 0.3 + 0.7 * wave
// (0x2000, the selected menu item) and "blink" is on while the wave is at
// least 0.5 (0x4000, PRESS START).
enum class Animation { none, fade, pulse, blink };

[[nodiscard]] float animation_alpha(Animation animation, double seconds);

// Copies of a sprite along one axis, optionally scrolling (the title's
// scanline strips: 20 copies 24 units apart, wrapping at 480).
struct SpriteRepeat {
  int count{1};
  std::array<float, 2> step{};
  float wrap{}; // positions wrap modulo this, on the stepped axis
  std::array<float, 2> scroll_per_second{};
  float scroll_reset{}; // the scroll offset restarts at 0 here
};

// One textured quad in 640x480 layout coordinates, as the data files give it.
struct Sprite {
  std::string id;
  std::string image;
  std::array<float, 4> rect{};              // left, top, right, bottom in texels
  std::array<float, 2> position{};          // top-left, or the centre when `centered`
  std::optional<std::array<float, 2>> size; // defaults to the rect's size
  bool centered{};
  int layer{0x101};               // draw order, lowest first (the original's layer word)
  std::uint32_t colour{0xffffff}; // multiplied with the texels, 0xRRGGBB
  Animation animation{Animation::none};
  std::optional<SpriteRepeat> repeat;
  bool title_only{}; // title screen: hidden once the menu is open
};

struct Quad {
  float x{}, y{}, width{}, height{};
};

// Throws std::runtime_error when a required value is missing or wrong.
[[nodiscard]] Sprite parse_sprite(const json::JsonValue &value);
[[nodiscard]] std::vector<Sprite> parse_sprites(const json::JsonValue &array);
// "#rrggbb".
[[nodiscard]] std::uint32_t parse_colour(const std::string &text);

// Where the sprite's copies are at `seconds` (one quad unless it repeats).
[[nodiscard]] std::vector<Quad> place_sprite(const Sprite &sprite, double seconds);

} // namespace phantomcrash
