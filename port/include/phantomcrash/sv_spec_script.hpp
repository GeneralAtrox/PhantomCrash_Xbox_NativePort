#pragma once

#include "phantomcrash/garage.hpp"
#include "phantomcrash/glyph_font.hpp"
#include "phantomcrash/input.hpp"
#include "phantomcrash/json.hpp"
#include "phantomcrash/sprite_layer.hpp"
#include "phantomcrash/town_screen.hpp"

#include <array>
#include <cstdint>
#include <string>
#include <vector>

namespace phantomcrash {

// A line of SV SPEC's vehicle page (0x03bc25).
enum class SpecStat {
  armour,    // TOTAL DURABILITY (0x076114)
  walk,      // RUN SPEED, km/h (0x07c27d)
  dash,      // MAX SPEED, km/h
  jump,      // JUMP HEIGHT, m
  overclock, // TOTAL O.C TIME: the modules' active times added, seconds (0x05f8b3)
  weight,    // TOTAL WEIGHT, t (0x075f93)
  capacity,  // WEIGHT CAPACITY: the legs', t
  sockets,   // SOCKET HOLE: the body's (0x05f97c)
};

struct SpecRow {
  SpecStat stat{SpecStat::armour};
  int decimals{};
};

// One of the two pages: its panel and the help line saying how to turn to
// the other.
struct SpecPage {
  Sprite panel;
  std::string help;
};

// SV SPEC (scene 36): what data/ui/sv_spec.json holds.
struct SvSpecScript : TownBasics {
  Buttons left{};
  Buttons right{};
  std::string page_sound;
  std::string back_sound;

  // The SV's and its CHIP's names in the window (0x03ba94).
  std::array<float, 2> name_at{};
  std::array<float, 2> chip_name_at{};
  std::uint32_t name_colour{0xffffff};
  int name_layer{};
  int name_glyphs{9};
  // The body maker's badge, by maker.
  std::array<float, 2> maker_at{};
  std::vector<Sprite> maker_icons;
  std::array<Sprite, 2> arrows;
  std::vector<Sprite> close_hint;
  std::array<SpecPage, 2> pages;

  // Page 0: the lines right-aligned from `top`, `step` apart.
  std::vector<SpecRow> rows;
  float values_right{};
  float values_top{};
  float values_step{};
  std::uint32_t value_colour{0xffffff};
  int value_layer{};

  // Page 1: each slot's weapon at `weapons_x`, the rows' heights; "none"
  // in its own colour for an empty slot.
  std::vector<PartSlot> weapon_slots;
  std::vector<float> weapon_rows;
  float weapons_x{};
  std::uint32_t weapon_colour{0xffffff};
  std::string none_text;
  std::uint32_t none_colour{0xffffff};
  int weapon_layer{};

  TickerStyle help_style;
};

// Throws std::runtime_error naming the member at fault.
[[nodiscard]] SvSpecScript parse_sv_spec(const json::JsonValue &document);

} // namespace phantomcrash
