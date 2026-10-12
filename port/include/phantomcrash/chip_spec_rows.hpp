#pragma once

// The CHIP dialog's fire-control panel laid out as rows (a port addition:
// the dialog's "rows" in data/ui/chip_shop.json and uplift.json). The
// window, the reticle preview and the stats box grow taller, and the stats
// box lists one line a row: each reticle's range, tinted with its colour in
// the preview instead of naming its shape, then the CHIP's stats, every value
// from its record at the level shown.

#include "phantomcrash/json.hpp"
#include "phantomcrash/sprite_layer.hpp"

#include <array>
#include <cstddef>
#include <cstdint>
#include <optional>
#include <string>
#include <vector>

namespace phantomcrash {

struct ChipRecord;

enum class ChipStat { mortal_shot, repulsion_shield, lock_time, aim_speed, knockdown_resistance };
enum class ChipSight { inner, outer };

struct ChipSpecRow {
  enum class Kind { range, stat, divider };
  Kind kind{Kind::stat};
  std::string label;
  std::array<float, 2> ink{}; // the label's ink top and bottom below the row's top
  ChipSight sight{};          // a range row's reticle
  std::uint32_t tint{};       // a range row's wash and strip
  ChipStat stat{};
  std::string unit;
  std::array<float, 2> unit_ink{};
  int decimals{}; // a stat's digits after the point
};

struct ChipSpecRows {
  float raise{}; // the window's top moves up this far
  float grow{};  // and the window, preview and stats box are this much taller
  // Art rows kept whole at the top and bottom while the middle stretches.
  std::array<float, 2> window_caps{};
  std::array<float, 2> preview_caps{};
  std::array<float, 2> box_caps{};
  std::array<float, 2> box_plain{}; // the stats box's empty rows, stretched between its caps
  std::array<float, 2> line{};      // its separator's rows
  std::array<float, 2> divider{};   // its divider's rows
  std::size_t preview_pop{};        // which of the dialog's pops is which
  std::size_t box_pop{1};
  float rows_top{}; // the first row's top below the stats box art's top
  float row_height{};
  float divider_height{};
  std::string font; // a data/ui/sprite_text.json display font
  std::uint32_t label_colour{0xffffff};
  std::array<float, 2> label_left{}; // a stat row's label, a range row's
  std::array<float, 2> axis_ink{};   // the X and Y
  std::array<float, 2> x_at{};       // the X's left and its value's right edge
  std::array<float, 2> y_at{};
  float value_right{};
  float unit_left{};
  float digit_scale{1.0F}; // the number font's digits drawn this size
  float digit_top{};
  std::uint32_t value_colour{0xffffff};
  // Uplifting, values that differ from the owned level's in this colour.
  std::optional<std::uint32_t> changed_colour;
  Sprite swatch;                     // a plain white patch, tinted for the washes and strips
  std::array<float, 2> tint_span{};  // the wash's left and right in the box
  std::array<float, 2> tint_alpha{}; // the wash's and the strip's
  float strip_width{};
  std::vector<ChipSpecRow> rows;
};

[[nodiscard]] ChipSpecRows parse_chip_spec_rows(const json::JsonValue &value);

// A stat row's value at `level`, times ten to the row's decimals, as the
// number font writes it.
[[nodiscard]] std::int64_t chip_stat_value(const ChipRecord &chip, const ChipSpecRow &row,
                                           int level);

} // namespace phantomcrash
