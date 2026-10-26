// Parsing of tables/slai_rumble.json (slai/rumble_table.py).

#include "phantomcrash/slai_rumble.hpp"

#include "phantomcrash/json_fields.hpp"

#include <array>
#include <stdexcept>
#include <string_view>
#include <utility>

namespace phantomcrash {
namespace {

using json::JsonValue;
using json::member;
using json::required;

// The u16 at +0x20 is read signed (lh, 0x24b460): 0xffff has no target.
constexpr int u16_sign_bit = 0x8000;
constexpr int u16_range = 0x10000;

constexpr std::array<std::pair<std::string_view, SlaiSpecialRule>, 8> special_names{{
    {"rental only", SlaiSpecialRule::rental_only},
    {"no optical camouflage", SlaiSpecialRule::no_optical_camouflage},
    {"shoulder weapons only", SlaiSpecialRule::shoulder_weapons_only},
    {"arm weapons only", SlaiSpecialRule::arm_weapons_only},
    {"close-range weapons only", SlaiSpecialRule::close_range_only},
    {"lock-on weapons only", SlaiSpecialRule::lock_on_only},
    {"grenade weapons only", SlaiSpecialRule::grenade_only},
    {"E-Chip only", SlaiSpecialRule::e_chip_only},
}};

int integer(const JsonValue &value) {
  return static_cast<int>(value.number());
}

std::vector<int> integers(const JsonValue &value) {
  std::vector<int> out;
  for (const auto &item : value.array()) {
    out.push_back(integer(item));
  }
  return out;
}

std::array<int, 2> pair_of(const JsonValue &value) {
  const auto &items = value.array();
  if (items.size() != 2) {
    throw std::runtime_error("needs a [day, night] pair");
  }
  return {integer(items[0]), integer(items[1])};
}

SlaiSpecialRule special_from(const JsonValue &value) {
  if (value.is_null()) {
    return SlaiSpecialRule::none;
  }
  for (const auto &[name, rule] : special_names) {
    if (value.string() == name) {
      return rule;
    }
  }
  throw std::runtime_error("unknown special rule \"" + value.string() + "\"");
}

int signed_target(const int raw) {
  return raw >= u16_sign_bit ? raw - u16_range : raw;
}

SlaiRegulation regulation_from(const JsonValue &value) {
  SlaiRegulation rule;
  rule.id = integer(required(value, "id"));
  rule.name = value.string_or("name", "");
  rule.entry_requirement = static_cast<std::int64_t>(required(value, "entry_requirement").number());
  const auto &maker = required(value, "maker");
  rule.maker = maker.is_null() ? slai_any_maker : integer(maker);
  for (const auto &leg_class : required(value, "leg_classes").array()) {
    rule.leg_classes.push_back(leg_class.string());
  }
  rule.special = member(value, "special", special_from);
  rule.target = signed_target(integer(required(value, "param_20")));
  rule.prize = static_cast<std::int64_t>(required(value, "prize").number());
  return rule;
}

SlaiMatch match_from(const JsonValue &value) {
  SlaiMatch match;
  match.id = integer(required(value, "id"));
  match.regulation = integer(required(value, "regulation"));
  match.night = required(value, "night").boolean();
  match.robots_min = integer(required(value, "robots_min"));
  match.robots_max = integer(required(value, "robots_max"));
  match.pool = integer(required(value, "level"));
  match.save_record = integer(required(value, "save_record"));
  match.save_record_b = integer(required(value, "save_record_b"));
  return match;
}

SlaiFinalRound round_from(const JsonValue &value) {
  SlaiFinalRound round;
  round.name = value.string_or("name", "");
  round.host = integer(required(value, "host"));
  round.matches = member(value, "matches", pair_of);
  round.host_ignores_rota = required(value, "host_ignores_rota").boolean();
  for (const auto &rule : required(value, "replace").array()) {
    SlaiFinalReplace replace;
    replace.from = member(rule, "from", integers);
    if (const auto &with = required(rule, "with"); !with.is_null()) {
      replace.with = pair_of(with);
    }
    round.replace.push_back(std::move(replace));
  }
  return round;
}

void check_rota(const SlaiRumbleTable &table) {
  const auto cities = table.cities.size();
  if (table.day_rows.size() != static_cast<std::size_t>(table.schedule_days)) {
    throw std::runtime_error("rota.days needs schedule_days rows");
  }
  for (const auto &row : table.day_rows) {
    if (row.size() != cities) {
      throw std::runtime_error("rota.days needs a city-day row for each city");
    }
    for (const int city_day : row) {
      if (city_day < 0 || city_day >= static_cast<int>(table.city_days.size())) {
        throw std::runtime_error("rota.days names a missing city-day row");
      }
    }
  }
  for (const auto &stages : table.city_days) {
    if (stages.size() != static_cast<std::size_t>(table.stages)) {
      throw std::runtime_error("rota.city_days needs a pair for each stage");
    }
  }
  if (table.finals_fallback.size() != cities) {
    throw std::runtime_error("finals.fallback needs a pair for each city");
  }
}

} // namespace

SlaiRumbleTable parse_slai_rumble_table(const JsonValue &document) {
  SlaiRumbleTable table;
  for (const auto &city : required(document, "cities").array()) {
    table.cities.push_back(required(city, "name").string());
  }
  table.display_order = member(document, "display_order", integers);
  table.schedule_days = integer(required(document, "schedule_days"));
  table.stages = integer(required(document, "stages"));
  if (table.schedule_days <= 0 || table.stages <= 0 || table.cities.empty()) {
    throw std::runtime_error("schedule_days, stages and cities must not be empty");
  }
  for (const auto &value : required(document, "regulations").array()) {
    table.regulations.push_back(regulation_from(value));
  }
  for (const auto &value : required(document, "matches").array()) {
    table.matches.push_back(match_from(value));
  }
  const auto &rota = required(document, "rota");
  for (const auto &day : required(rota, "days").array()) {
    table.day_rows.push_back(member(day, "city_days", integers));
  }
  for (const auto &row : required(rota, "city_days").array()) {
    if (integer(required(row, "id")) != static_cast<int>(table.city_days.size())) {
      throw std::runtime_error("rota.city_days is out of order");
    }
    auto &stages = table.city_days.emplace_back();
    for (const auto &stage : required(row, "stages").array()) {
      stages.push_back(pair_of(stage));
    }
  }
  const auto &finals = required(document, "finals");
  table.finals_stage = integer(required(finals, "stage"));
  for (const auto &round : required(finals, "rounds").array()) {
    table.finals.push_back(round_from(round));
  }
  for (const auto &pair : required(finals, "fallback").array()) {
    table.finals_fallback.push_back(pair_of(pair));
  }
  for (std::size_t index = 0; index < table.regulations.size(); ++index) {
    if (table.regulations[index].id != static_cast<int>(index)) {
      throw std::runtime_error("regulations are out of order");
    }
  }
  for (std::size_t index = 0; index < table.matches.size(); ++index) {
    const auto &match = table.matches[index];
    if (match.id != static_cast<int>(index) ||
        slai_regulation(table, match.regulation) == nullptr) {
      throw std::runtime_error("match " + std::to_string(index) +
                               " is out of order or names "
                               "a missing regulation");
    }
  }
  check_rota(table);
  if (document.find("opponents") != nullptr) {
    table.opponents = member(document, "opponents", parse_slai_opponents);
  }
  if (document.find("parts") != nullptr) {
    table.parts = member(document, "parts", parse_slai_part_facts);
  }
  return table;
}

const SlaiMatch *slai_match(const SlaiRumbleTable &table, const int id) {
  return id >= 0 && id < static_cast<int>(table.matches.size())
             ? &table.matches[static_cast<std::size_t>(id)]
             : nullptr;
}

const SlaiRegulation *slai_regulation(const SlaiRumbleTable &table, const int id) {
  return id >= 0 && id < static_cast<int>(table.regulations.size())
             ? &table.regulations[static_cast<std::size_t>(id)]
             : nullptr;
}

} // namespace phantomcrash
