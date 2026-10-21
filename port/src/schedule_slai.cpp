// Parsing of data/ui/schedule.json's "slai" (phantomcrash/schedule_slai.hpp).

#include "phantomcrash/schedule_slai.hpp"

#include "phantomcrash/json_fields.hpp"

#include <algorithm>
#include <stdexcept>

namespace phantomcrash {
namespace {

using json::member;
using json::numbers;
using json::required;

std::array<float, 4> rect4(const json::JsonValue &value) {
  return numbers<4>(value, "rect");
}

std::array<float, 2> point2(const json::JsonValue &value) {
  return numbers<2>(value, "point");
}

float number(const json::JsonValue &value, const char *name) {
  return static_cast<float>(required(value, name).number());
}

SlaiScheduleBadge parse_badge(const json::JsonValue &value) {
  SlaiScheduleBadge badge;
  if (const auto *regulations = value.find("regulations")) {
    for (const auto &id : regulations->array()) {
      badge.regulations.push_back(static_cast<int>(id.number()));
    }
  }
  badge.rect = member(value, "rect", rect4);
  badge.class_text = required(value, "class").string();
  return badge;
}

void parse_toggle(const json::JsonValue &value, SlaiScheduleScript &script) {
  script.toggle = member(value, "normal", parse_sprite);
  script.toggle_lit = member(value, "selected", parse_sprite);
  script.back_toggle = member(value, "back_normal", parse_sprite);
  script.back_toggle_lit = member(value, "back_selected", parse_sprite);
  const auto &places = required(value, "buttons");
  script.skip_at = member(places, "skip", point2);
  script.toggle_at = member(places, "toggle", point2);
  script.exit_at = member(places, "exit", point2);
}

void parse_cells(const json::JsonValue &value, SlaiScheduleScript &script) {
  script.cell_origin = member(value, "origin", point2);
  script.cell_step = member(value, "step", point2);
  script.day = member(value, "day", rect4);
  script.day_lit = member(value, "day_lit", rect4);
  script.night = member(value, "night", rect4);
  script.night_lit = member(value, "night_lit", rect4);
  script.closed = member(value, "closed", rect4);
  script.closed_lit = member(value, "closed_lit", rect4);
  script.clear_rect = member(value, "clear", rect4);
  script.clear_box = member(value, "clear_box", rect4);
}

void parse_badges(const json::JsonValue &value, SlaiScheduleScript &script) {
  script.badge_height = number(value, "height");
  script.badge_width = number(value, "width");
  script.badge_inset = number(value, "inset");
  script.badge_dim = parse_colour(required(value, "dim").string());
  for (const auto &badge : required(value, "list").array()) {
    script.badges.push_back(parse_badge(badge));
  }
}

} // namespace

SlaiScheduleLabel parse_slai_label(const json::JsonValue &value) {
  SlaiScheduleLabel label;
  label.font = required(value, "font").string();
  label.box = member(value, "box", rect4);
  label.colour = parse_colour(required(value, "colour").string());
  label.layer = static_cast<int>(value.number_or("layer", label.layer));
  if (const auto *shadow = value.find("shadow")) {
    label.shadow = parse_text_shadow(*shadow);
    label.shadow_blur = static_cast<float>(shadow->number_or("blur", 0.0));
  }
  return label;
}

SlaiSchedulePlate parse_slai_plate(const json::JsonValue &value) {
  SlaiSchedulePlate plate;
  plate.box = member(value, "box", rect4);
  plate.outlines = parse_shape_outlines(required(value, "outlines"));
  plate.layer = static_cast<int>(value.number_or("layer", plate.layer));
  return plate;
}

SlaiScheduleScript parse_slai_schedule(const json::JsonValue &value) {
  try {
    SlaiScheduleScript script;
    script.enabled = true;
    parse_toggle(required(value, "toggle"), script);
    for (const auto &id : required(value, "hidden_layers").array()) {
      script.hidden_layers.push_back(id.string());
    }
    script.header = member(value, "header", parse_sprite);
    const auto &digits = required(value, "day_digits");
    script.tens = member(digits, "tens", point2);
    script.units = member(digits, "units", point2);
    script.cursor_day_y = number(value, "cursor_day_y");
    parse_cells(required(value, "cells"), script);
    parse_badges(required(value, "badges"), script);
    const auto &cursor = required(value, "cursor");
    script.cursor_border = number(cursor, "border");
    script.cursor_offset = member(cursor, "offset", point2);
    script.cursor_size = member(cursor, "size", point2);
    script.grid = member(value, "grid", parse_slai_plate);
    script.plate = member(value, "plate", parse_slai_plate);
    script.city = member(value, "city", parse_slai_label);
    script.corner = member(value, "corner", parse_slai_plate);
    script.title = member(value, "title", parse_slai_label);
    script.title_text = required(value, "title_text").string();
    for (const auto &name : required(value, "city_names").array()) {
      script.city_names.push_back(name.string());
    }
    if (script.cell_step[0] <= 0 || script.cell_step[1] <= 0 || script.badge_height <= 0 ||
        script.badges.empty()) {
      throw std::runtime_error("needs a cell step above 0, a badge height and badges");
    }
    return script;
  } catch (const std::exception &error) {
    throw std::runtime_error(std::string("slai: ") + error.what());
  }
}

const SlaiScheduleBadge *slai_schedule_badge(const SlaiScheduleScript &script,
                                             const int regulation) {
  const SlaiScheduleBadge *fallback = nullptr;
  for (const SlaiScheduleBadge &badge : script.badges) {
    if (badge.regulations.empty()) {
      fallback = fallback != nullptr ? fallback : &badge;
    } else if (std::ranges::find(badge.regulations, regulation) != badge.regulations.end()) {
      return &badge;
    }
  }
  return fallback;
}

} // namespace phantomcrash
