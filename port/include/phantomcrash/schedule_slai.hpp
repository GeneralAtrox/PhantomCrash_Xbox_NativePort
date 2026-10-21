#pragma once

// The schedule's S.L.A.I. page (docs/SLAI_RUMBLE_PLAN.md, "Screens"): a
// port addition offered when S.L.A.I. is installed and enabled. A toggle
// between SKIP TO DAY and EXIT turns from the Phantom Crash calendar to
// S.L.A.I.'s and back: one thinner row per city in the original's display
// order, each cell the city's rumble at the player's stage on that day
// (data/world/slai_rota.json) or CLOSED. Everything is drawn from the
// schedule's own art; the city plates, grid lines and labels are sharp
// shapes and text. See data/ui/schedule.json's "slai".

#include "phantomcrash/json.hpp"
#include "phantomcrash/sprite_layer.hpp"
#include "phantomcrash/sprite_shapes.hpp"
#include "phantomcrash/sprite_text.hpp"

#include <array>
#include <cstdint>
#include <optional>
#include <string>
#include <vector>

namespace phantomcrash {

// A badge for the regulations it lists (none: every other one), its rect
// the badge's ink alone, and the class the INFORMATION popup names.
struct SlaiScheduleBadge {
  std::vector<int> regulations;
  std::array<float, 4> rect{};
  std::string class_text;
};

// Sharp lettering fitted to a box (canvas units, relative to what holds it).
struct SlaiScheduleLabel {
  std::string font;
  std::array<float, 4> box{};
  std::uint32_t colour{0xffffff};
  std::optional<TextShadow> shadow;
  float shadow_blur{};
  int layer{0x104};
};

// Vector outlines (sprite_shapes.json's format) over a box.
struct SlaiSchedulePlate {
  std::array<float, 4> box{};
  std::vector<ShapeOutline> outlines;
  int layer{0x104};
};

struct SlaiScheduleScript {
  bool enabled{}; // false without a "slai" entry: no toggle

  // The toggle's art and lettering on each page (sprite_text.json ids):
  // on the Phantom Crash page it names S.L.A.I., and the other way round.
  Sprite toggle, toggle_lit, back_toggle, back_toggle_lit;
  // Where the bottom row's buttons stand while the toggle is offered.
  std::array<float, 2> skip_at{}, toggle_at{}, exit_at{};

  // The town layers the page does not draw (the Phantom Crash grid).
  std::vector<std::string> hidden_layers;
  Sprite header;                        // the grid's DAY- strip, over the columns
  std::array<float, 2> tens{}, units{}; // the first column's day digits
  float cursor_day_y{};
  std::array<float, 2> cell_origin{}, cell_step{};
  // The venue's tiles, cut to their strip; CLOSED's middle strip.
  std::array<float, 4> day{}, day_lit{}, night{}, night_lit{}, closed{}, closed_lit{};
  // A badge fitted to this height (and no wider than badge_width) at the
  // cell's left, badge_inset in; dimmed off the cursor's column.
  float badge_height{}, badge_width{}, badge_inset{};
  std::uint32_t badge_dim{0x808080};
  std::vector<SlaiScheduleBadge> badges;
  // CLEAR fitted inside the cell once the match's record is cleared.
  std::array<float, 4> clear_rect{};
  std::array<float, 4> clear_box{}; // cell-relative
  // The cursor's frame and pulse cut into nine with this border and
  // stretched over the cell, offset and sized.
  float cursor_border{};
  std::array<float, 2> cursor_offset{}, cursor_size{};
  SlaiSchedulePlate grid;   // the lines, on the canvas
  SlaiSchedulePlate plate;  // each row's city plate; x on the canvas, y the row's
  SlaiScheduleLabel city;   // its name, in the plate's box
  SlaiSchedulePlate corner; // the page's own name over the plates
  SlaiScheduleLabel title;  // in the corner's box
  std::string title_text;
  std::vector<std::string> city_names; // by city; the table's name past the end
};

// Throw std::runtime_error naming the member at fault.
[[nodiscard]] SlaiScheduleScript parse_slai_schedule(const json::JsonValue &value);
// "font", "box", "colour", "layer" and an optional "shadow" with "blur".
[[nodiscard]] SlaiScheduleLabel parse_slai_label(const json::JsonValue &value);
// "box", "outlines" and "layer".
[[nodiscard]] SlaiSchedulePlate parse_slai_plate(const json::JsonValue &value);

// The badge for a regulation: the first listing it, else the catch-all.
[[nodiscard]] const SlaiScheduleBadge *slai_schedule_badge(const SlaiScheduleScript &script,
                                                           int regulation);

} // namespace phantomcrash
