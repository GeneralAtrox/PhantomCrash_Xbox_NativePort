// Parsing of data/ui/slai_rumble.json (phantomcrash/slai_rumble_hub.hpp).

#include "phantomcrash/slai_rumble_hub.hpp"

#include "phantomcrash/json_fields.hpp"

#include <algorithm>
#include <stdexcept>

namespace phantomcrash {
namespace {

using json::member;
using json::numbers;
using json::required;

TexelRect rect4(const json::JsonValue &value) {
  return numbers<4>(value, "rect");
}

std::array<float, 2> point2(const json::JsonValue &value) {
  return numbers<2>(value, "point");
}

std::vector<std::string> strings(const json::JsonValue &value) {
  std::vector<std::string> out;
  for (const auto &item : value.array()) {
    out.push_back(item.string());
  }
  return out;
}

void parse_rows(const json::JsonValue &value, SlaiRumbleHubScript &script) {
  script.cell_image = required(value, "image").string();
  script.row_origin = member(value, "origin", point2);
  script.row_step = member(value, "step", point2);
  script.tile_at = member(value, "tile", point2);
  const auto &tiles = required(value, "tiles");
  script.tiles = {member(tiles, "day", rect4), member(tiles, "day_lit", rect4),
                  member(tiles, "night", rect4), member(tiles, "night_lit", rect4)};
  script.tile_layer = static_cast<int>(value.number_or("tile_layer", script.tile_layer));
  script.badge_layer = static_cast<int>(value.number_or("badge_layer", script.badge_layer));
  script.badge_dim = parse_colour(required(value, "badge_dim").string());
  for (const auto &badge : required(value, "badges").array()) {
    SlaiHubBadge entry;
    if (const auto *regulations = badge.find("regulations")) {
      for (const auto &id : regulations->array()) {
        entry.regulations.push_back(static_cast<int>(id.number()));
      }
    }
    entry.rect = member(badge, "rect", rect4);
    entry.class_text = required(badge, "class").string();
    script.badges.push_back(std::move(entry));
  }
  script.clear = member(value, "clear", parse_sprite);
  script.plate = member(value, "plate", parse_slai_plate);
  script.city = member(value, "city", parse_slai_label);
  script.rule = member(value, "rule", parse_slai_label);
}

void parse_city_art(const json::JsonValue &value, SlaiRumbleHubScript &script) {
  script.emblem_box = member(value, "emblem_box", rect4);
  script.art_label_at = member(value, "label_at", point2);
  script.art_label_scale = static_cast<float>(value.number_or("label_scale", 1.0));
  script.art_layer = static_cast<int>(value.number_or("layer", script.art_layer));
  for (const auto &city : required(value, "cities").array()) {
    script.city_art.push_back({required(city, "image").string(), member(city, "emblem", rect4),
                               member(city, "day", rect4), member(city, "night", rect4)});
  }
}

void parse_amounts(const json::JsonValue &value, SlaiRumbleHubScript &script) {
  script.entry_label = member(value, "entry_label", parse_slai_label);
  script.prize_label = member(value, "prize_label", parse_slai_label);
  script.entry_text = required(value, "entry_text").string();
  script.prize_text = required(value, "prize_text").string();
  script.entry_at = member(value, "entry_at", point2);
  script.prize_at = member(value, "prize_at", point2);
  script.amount_digits = static_cast<int>(value.number_or("digits", script.amount_digits));
  script.amount_colour = parse_colour(required(value, "colour").string());
  script.amount_layer = static_cast<int>(value.number_or("layer", script.amount_layer));
}

} // namespace

SlaiRumbleHubScript parse_slai_rumble_hub(const json::JsonValue &document) {
  try {
    SlaiRumbleHubScript script;
    parse_town_basics(document, script);
    const auto &buttons = required(document, "buttons");
    script.up = buttons_from_json(required(buttons, "up"));
    script.down = buttons_from_json(required(buttons, "down"));
    const auto &sounds = required(document, "sounds");
    script.close_sound = sounds.string_or("close", "");
    script.refuse_sound = sounds.string_or("refuse", "");
    const auto &exit = required(document, "exit");
    script.exit = member(exit, "normal", parse_sprite);
    script.exit_selected = member(exit, "selected", parse_sprite);
    parse_rows(required(document, "rows"), script);
    parse_amounts(required(document, "amounts"), script);
    if (const auto *art = document.find("city_art")) {
      parse_city_art(*art, script);
    }
    const auto &cursor = required(document, "cursor");
    script.cursor_frame = member(cursor, "frame", parse_sprite);
    script.cursor_pulse = member(cursor, "pulse", parse_sprite);
    script.cursor_border = static_cast<float>(required(cursor, "border").number());
    script.cursor_box = member(cursor, "box", rect4);
    script.info = member(document, "info", parse_contest_info);
    script.city_names = strings(required(document, "city_names"));
    script.closed = strings(required(document, "closed"));
    const auto &refusals = required(document, "refusals");
    script.short_of_money = strings(required(refusals, "money"));
    script.not_ready = strings(required(refusals, "not_ready"));
    script.wrong_maker = strings(required(refusals, "maker"));
    script.wrong_legs = strings(required(refusals, "legs"));
    script.special_unmet = strings(required(refusals, "special"));
    script.no_rental = strings(required(refusals, "rental"));
    script.battle = required(document, "battle").string();
    for (const auto &[key, list] : {std::pair{"records", &script.records},
                                    std::pair{"exclude_records", &script.exclude_records}}) {
      if (const auto *items = document.find(key)) {
        for (const auto &item : items->array()) {
          list->push_back(static_cast<int>(item.number()));
        }
      }
    }
    script.one_per_match = document.boolean_or("one_per_match", false);
    if (script.row_step[1] <= 0 || script.badges.empty()) {
      throw std::runtime_error("needs a row step above 0 and badges");
    }
    return script;
  } catch (const std::exception &error) {
    throw std::runtime_error(std::string("slai_rumble.json: ") + error.what());
  }
}

const SlaiHubBadge *slai_hub_badge(const SlaiRumbleHubScript &script, const int regulation) {
  const SlaiHubBadge *fallback = nullptr;
  for (const SlaiHubBadge &badge : script.badges) {
    if (badge.regulations.empty()) {
      fallback = fallback != nullptr ? fallback : &badge;
    } else if (std::ranges::find(badge.regulations, regulation) != badge.regulations.end()) {
      return &badge;
    }
  }
  return fallback;
}

} // namespace phantomcrash
