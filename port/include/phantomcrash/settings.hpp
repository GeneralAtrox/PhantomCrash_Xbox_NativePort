#pragma once

#include "phantomcrash/frame_rate.hpp"
#include "phantomcrash/input.hpp"
#include "phantomcrash/json.hpp"

#include <array>
#include <cstddef>
#include <filesystem>
#include <string_view>

namespace phantomcrash {

// The fighting actions BUTTON CONFIG assigns, in the original's order
// (0x1dd7e0 rows 3-10), and the controller buttons it assigns them to
// (masks 0x1dd4e0: A, B, X, Y, L, R, white, black).
inline constexpr std::size_t control_action_count = 8;
inline constexpr std::array<std::string_view, control_action_count> control_actions{
    "right_arm_weapons",
    "left_arm_weapons",
    "right_backpack_weapons",
    "left_backpack_weapons",
    "right_step",
    "left_step",
    "jump",
    "optical_camouflage"};
inline constexpr std::array<std::string_view, control_action_count> bindable_button_names{
    "a", "b", "x", "y", "left_trigger", "right_trigger", "white", "black"};
inline constexpr std::array<Buttons, control_action_count> bindable_buttons{
    button::a,     button::b,    button::x, button::y, button::left_trigger, button::right_trigger,
    button::white, button::black};

// The index of an action or bindable button name, or -1.
[[nodiscard]] int control_action_index(std::string_view name);
[[nodiscard]] int bindable_button_index(std::string_view name);

// BUTTON CONFIG (the original keeps it at +0x6018 of its config).
struct ControlOptions {
  int type{};     // CONTROL TYPE: 0 Standard, 1 Preferred
  int view{};     // VIEW CHANGE TYPE: 0 normal, 1 reverse
  int beginner{}; // BEGINNER MODE: 0 off, 1 on
  // Action -> bindable button; always a permutation (0x1dd500 is the default).
  std::array<int, control_action_count> bindings{5, 4, 1, 3, 7, 6, 0, 2};

  bool operator==(const ControlOptions &) const = default;
};

// Options the game's OPTION screen and its sub-screens change. The defaults
// are the original's (its DEFAULT item restores them all, 0x2e6ea).
struct GameOptions {
  bool vibration{true}; // the original keeps the opposite, "vibration off", in bit 30 of 0x9cc
  int bgm_volume{255};  // 0-255 (byte 0x9cb)
  int se_volume{225};   // 0-255 (low byte of 0x9cc)
  // Where the picture sits, in canvas units: every render pass's viewport
  // moves by it (0xb7cc0). SCREEN ADJUST sets it; bits 8-19 of 0x9cc.
  std::array<int, 2> screen_offset{};
  ControlOptions controls;

  bool operator==(const GameOptions &) const = default;
};

// The on/off option a data file names ("vibration"), or null.
[[nodiscard]] bool *game_option(GameOptions &options, std::string_view name);
// A numbered setting a data file names ("bgm_volume", "se_volume",
// "control_type", "view_change", "beginner"), or null.
[[nodiscard]] int *game_value(GameOptions &options, std::string_view name);
[[nodiscard]] const int *game_value(const GameOptions &options, std::string_view name);

// Desktop settings chosen from the window menu, kept between runs.
struct Settings {
  // VSync on and no cap by default: presentation follows the display. VSync
  // off with no cap is the unlimited mode.
  FrameRateSettings frame_rate{true, false, 60};
  bool fullscreen{};
  bool frame_counter{};
  bool pause_unfocused{true};
  GameOptions game;

  bool operator==(const Settings &) const = default;
};

// Missing members keep their defaults; out-of-range values are clamped, and
// bindings that are not a permutation fall back to the default.
[[nodiscard]] Settings settings_from_json(const json::JsonValue &value);
[[nodiscard]] json::JsonValue settings_to_json(const Settings &settings);

// Defaults when the file is missing or unreadable, so a bad file never stops
// the game from starting.
[[nodiscard]] Settings load_settings(const std::filesystem::path &path);
// Writes through a temporary file; returns false when the folder is not writable.
bool save_settings(const std::filesystem::path &path, const Settings &settings);

} // namespace phantomcrash
