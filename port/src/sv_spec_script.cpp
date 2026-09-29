// Parsing of data/ui/sv_spec.json.

#include "phantomcrash/sv_spec_script.hpp"

#include "phantomcrash/json_fields.hpp"
#include "phantomcrash/location_frame.hpp"
#include "phantomcrash/menu_screen.hpp"

#include <map>
#include <stdexcept>

namespace phantomcrash {
namespace {

using json::member;
using json::numbers;
using json::required;

float number_of(const json::JsonValue &value, const char *key) {
  return static_cast<float>(required(value, key).number());
}

int integer_of(const json::JsonValue &value, const char *key) {
  return static_cast<int>(required(value, key).number());
}

std::uint32_t colour_of(const json::JsonValue &value, const char *key) {
  return parse_colour(required(value, key).string());
}

SpecStat stat_named(const std::string &name) {
  static const std::map<std::string, SpecStat> stats{
      {"armour", SpecStat::armour},       {"walk", SpecStat::walk},
      {"dash", SpecStat::dash},           {"jump", SpecStat::jump},
      {"overclock", SpecStat::overclock}, {"weight", SpecStat::weight},
      {"capacity", SpecStat::capacity},   {"sockets", SpecStat::sockets},
  };
  const auto found = stats.find(name);
  if (found == stats.end()) {
    throw std::runtime_error("no vehicle line is named " + name);
  }
  return found->second;
}

PartSlot slot_named(const std::string &name) {
  static const std::map<std::string, PartSlot> slots{
      {"right_arm", PartSlot::right_arm},
      {"left_arm", PartSlot::left_arm},
      {"right_shoulder", PartSlot::right_shoulder},
      {"left_shoulder", PartSlot::left_shoulder},
      // The older names, which called side 0 left.
      {"arm_left", PartSlot::right_arm},
      {"arm_right", PartSlot::left_arm},
      {"shoulder_left", PartSlot::right_shoulder},
      {"shoulder_right", PartSlot::left_shoulder},
  };
  const auto found = slots.find(name);
  if (found == slots.end()) {
    throw std::runtime_error("no weapon slot is named " + name);
  }
  return found->second;
}

void parse_window(const json::JsonValue &document, SvSpecScript &script) {
  const auto &names = required(document, "names");
  script.name_at = numbers<2>(required(names, "name_at"), "name_at");
  script.chip_name_at = numbers<2>(required(names, "chip_name_at"), "chip_name_at");
  script.name_colour = colour_of(names, "colour");
  script.name_layer = integer_of(names, "layer");
  script.name_glyphs = static_cast<int>(names.number_or("max_glyphs", 9));
  script.maker_at = numbers<2>(required(document, "maker_at"), "maker_at");
  script.maker_icons = member(document, "maker_icons", parse_sprites);
  const auto arrows = member(document, "arrows", parse_sprites);
  if (arrows.size() != 2) {
    throw std::runtime_error("arrows: needs a left and a right");
  }
  script.arrows = {arrows[0], arrows[1]};
  script.close_hint = member(document, "close_hint", parse_sprites);
  const auto &pages = required(document, "pages").array();
  if (pages.size() != script.pages.size()) {
    throw std::runtime_error("pages: needs the vehicle's and the weapons'");
  }
  for (std::size_t index = 0; index < pages.size(); ++index) {
    script.pages[index].panel = member(pages[index], "panel", parse_sprite);
    script.pages[index].help = required(pages[index], "help").string();
  }
}

void parse_vehicle(const json::JsonValue &value, SvSpecScript &script) {
  script.values_right = number_of(value, "right");
  script.values_top = number_of(value, "top");
  script.values_step = number_of(value, "step");
  script.value_colour = colour_of(value, "colour");
  script.value_layer = integer_of(value, "layer");
  for (const json::JsonValue &row : required(value, "rows").array()) {
    script.rows.push_back(
        {stat_named(required(row, "stat").string()), integer_of(row, "decimals")});
  }
}

void parse_weapons(const json::JsonValue &value, SvSpecScript &script) {
  script.weapons_x = number_of(value, "x");
  for (const json::JsonValue &row : required(value, "rows").array()) {
    script.weapon_rows.push_back(static_cast<float>(row.number()));
  }
  for (const json::JsonValue &slot : required(value, "slots").array()) {
    script.weapon_slots.push_back(slot_named(slot.string()));
  }
  if (script.weapon_slots.size() != script.weapon_rows.size()) {
    throw std::runtime_error("weapons: needs a row for each slot");
  }
  script.weapon_colour = colour_of(value, "colour");
  script.none_text = required(value, "none").string();
  script.none_colour = colour_of(value, "none_colour");
  script.weapon_layer = integer_of(value, "layer");
}

} // namespace

SvSpecScript parse_sv_spec(const json::JsonValue &document) {
  try {
    SvSpecScript script;
    parse_town_basics(document, script);
    const auto &buttons = required(document, "buttons");
    script.left = buttons_from_json(required(buttons, "left"));
    script.right = buttons_from_json(required(buttons, "right"));
    const auto &sounds = required(document, "sounds");
    script.page_sound = sounds.string_or("page", "");
    script.back_sound = sounds.string_or("back", "");
    parse_window(document, script);
    parse_vehicle(required(document, "vehicle"), script);
    parse_weapons(required(document, "weapons"), script);
    script.help_style = member(document, "help", parse_ticker);
    return script;
  } catch (const std::exception &error) {
    throw std::runtime_error(std::string("sv_spec.json: ") + error.what());
  }
}

} // namespace phantomcrash
