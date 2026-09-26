#pragma once

#include "phantomcrash/json.hpp"

#include <cstdint>
#include <optional>
#include <string_view>

namespace phantomcrash {

// Original Xbox controller buttons as a bit set. Keyboard keys and modern
// gamepads are mapped onto these by the desktop layer.
using Buttons = std::uint32_t;

namespace button {
inline constexpr Buttons a = 1U << 0;
inline constexpr Buttons b = 1U << 1;
inline constexpr Buttons x = 1U << 2;
inline constexpr Buttons y = 1U << 3;
inline constexpr Buttons black = 1U << 4;
inline constexpr Buttons white = 1U << 5;
inline constexpr Buttons start = 1U << 6;
inline constexpr Buttons back = 1U << 7;
inline constexpr Buttons up = 1U << 8;
inline constexpr Buttons down = 1U << 9;
inline constexpr Buttons left = 1U << 10;
inline constexpr Buttons right = 1U << 11;
inline constexpr Buttons left_trigger = 1U << 12;
inline constexpr Buttons right_trigger = 1U << 13;
inline constexpr Buttons left_thumb = 1U << 14;
inline constexpr Buttons right_thumb = 1U << 15;

// Groups the data files use. "confirm" is the original's A/START mask 0x110,
// "cancel" its B, and "any" every button.
inline constexpr Buttons confirm = a | start;
inline constexpr Buttons cancel = b | back;
inline constexpr Buttons directions = up | down | left | right;
inline constexpr Buttons any = (1U << 16) - 1;
} // namespace button

// One logic tick's view of the controllers.
struct InputFrame {
  Buttons held{};
  // Buttons that went down since the previous tick, including taps that were
  // released again before the tick sampled them.
  Buttons pressed{};
  int controllers{};

  [[nodiscard]] bool any_pressed(Buttons mask) const {
    return (pressed & mask) != 0;
  }
};

// Builds InputFrames from sampled button levels plus presses latched from
// events between ticks.
class InputEdges final {
public:
  InputFrame update(Buttons held, Buttons latched_presses, int controllers);

private:
  Buttons previous_{};
};

// A button or group name ("a", "start", "confirm", "cancel", "any", ...).
[[nodiscard]] std::optional<Buttons> button_from_name(std::string_view name);
// A name or an array of names from a data file; throws on an unknown name.
[[nodiscard]] Buttons buttons_from_json(const json::JsonValue &value);

} // namespace phantomcrash
