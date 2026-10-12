// Parsing of the CHIP dialog's "rows" (data/ui/chip_shop.json, uplift.json).

#include "phantomcrash/chip_spec_rows.hpp"

#include "phantomcrash/json_fields.hpp"
#include "phantomcrash/robot_parts.hpp"

#include <cmath>
#include <stdexcept>
#include <utility>

namespace phantomcrash {
namespace {

using json::numbers;
using json::required;

// A CHIP's lock_frames count 60 Hz frames; the panel shows seconds.
constexpr double lock_frames_per_second = 60.0;

float number_of(const json::JsonValue &value, const char *key) {
  return static_cast<float>(required(value, key).number());
}

std::array<float, 2> pair_of(const json::JsonValue &value, const char *key) {
  return numbers<2>(required(value, key), key);
}

ChipStat stat_named(const std::string &name) {
  static const std::pair<const char *, ChipStat> stats[]{
      {"mortal_shot", ChipStat::mortal_shot},
      {"repulsion_shield", ChipStat::repulsion_shield},
      {"lock_time", ChipStat::lock_time},
      {"aim_speed", ChipStat::aim_speed},
      {"knockdown_resistance", ChipStat::knockdown_resistance}};
  for (const auto &[key, stat] : stats) {
    if (name == key) {
      return stat;
    }
  }
  throw std::runtime_error("unknown CHIP stat " + name);
}

ChipSpecRow parse_row(const json::JsonValue &value) {
  ChipSpecRow row;
  if (value.find("divider") != nullptr) {
    row.kind = ChipSpecRow::Kind::divider;
    return row;
  }
  row.label = required(value, "label").string();
  row.ink = pair_of(value, "ink");
  if (const auto *sight = value.find("range")) {
    row.kind = ChipSpecRow::Kind::range;
    if (sight->string() != "inner" && sight->string() != "outer") {
      throw std::runtime_error("range must be inner or outer");
    }
    row.sight = sight->string() == "inner" ? ChipSight::inner : ChipSight::outer;
    row.tint = parse_colour(required(value, "tint").string());
    return row;
  }
  row.stat = stat_named(required(value, "stat").string());
  row.unit = value.string_or("unit", "");
  if (!row.unit.empty()) {
    row.unit_ink = pair_of(value, "unit_ink");
  }
  row.decimals = static_cast<int>(value.number_or("decimals", 0));
  if (row.decimals < 0 || row.decimals > 3) {
    throw std::runtime_error("decimals must be 0 to 3");
  }
  return row;
}

} // namespace

ChipSpecRows parse_chip_spec_rows(const json::JsonValue &value) {
  ChipSpecRows out;
  out.raise = number_of(value, "raise");
  out.grow = number_of(value, "grow");
  if (out.grow < 0.0F || out.raise < 0.0F || out.raise > out.grow) {
    throw std::runtime_error("rows: grow must be at least raise, and neither below 0");
  }
  out.window_caps = pair_of(value, "window_caps");
  out.preview_caps = pair_of(value, "preview_caps");
  out.box_caps = pair_of(value, "box_caps");
  out.box_plain = pair_of(value, "box_plain");
  out.line = pair_of(value, "line");
  out.divider = pair_of(value, "divider");
  out.preview_pop = static_cast<std::size_t>(required(value, "preview_pop").number());
  out.box_pop = static_cast<std::size_t>(required(value, "box_pop").number());
  out.rows_top = number_of(value, "rows_top");
  out.row_height = number_of(value, "row_height");
  out.divider_height = number_of(value, "divider_height");
  out.font = required(value, "font").string();
  out.label_colour = parse_colour(required(value, "label_colour").string());
  out.label_left = pair_of(value, "label_left");
  out.axis_ink = pair_of(value, "axis_ink");
  out.x_at = pair_of(value, "x_at");
  out.y_at = pair_of(value, "y_at");
  out.value_right = number_of(value, "value_right");
  out.unit_left = number_of(value, "unit_left");
  out.digit_scale = number_of(value, "digit_scale");
  out.digit_top = number_of(value, "digit_top");
  out.value_colour = parse_colour(required(value, "value_colour").string());
  if (const auto *raised = value.find("changed_colour")) {
    out.changed_colour = parse_colour(raised->string());
  }
  out.swatch = parse_sprite(required(value, "swatch"));
  out.tint_span = pair_of(value, "tint_span");
  out.tint_alpha = pair_of(value, "tint_alpha");
  out.strip_width = number_of(value, "strip_width");
  for (const auto &row : required(value, "rows").array()) {
    out.rows.push_back(parse_row(row));
  }
  return out;
}

std::int64_t chip_stat_value(const ChipRecord &chip, const ChipSpecRow &row, const int level) {
  double value = 0.0;
  switch (row.stat) {
  case ChipStat::mortal_shot:
    value = chip.critical_percent.value(level);
    break;
  case ChipStat::repulsion_shield:
    value = chip.dodge_percent.value(level);
    break;
  case ChipStat::lock_time:
    value = chip.lock_frames / lock_frames_per_second;
    break;
  case ChipStat::aim_speed:
    value = chip.track;
    break;
  case ChipStat::knockdown_resistance:
    value = chip.knockdown_resistance.value(level);
    break;
  }
  return std::llround(value * std::pow(10.0, row.decimals));
}

} // namespace phantomcrash
