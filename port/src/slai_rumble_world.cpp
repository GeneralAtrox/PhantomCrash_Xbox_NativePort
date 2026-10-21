// Parsing of data/world/slai_rumble.json, and loading S.L.A.I. Rumble's
// table and world data through the data roots.

#include "phantomcrash/slai_rumble.hpp"

#include "phantomcrash/data_roots.hpp"
#include "phantomcrash/json_fields.hpp"

#include <stdexcept>
#include <utility>

namespace phantomcrash {
namespace {

using json::JsonValue;
using json::member;
using json::required;

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

std::array<int, 2> range_of(const JsonValue &value) {
  const auto values = integers(value);
  if (values.size() != 2 || values[0] > values[1]) {
    throw std::runtime_error("needs [first, one past the last]");
  }
  return {values[0], values[1]};
}

SlaiStatusRule status_rule_from(const JsonValue &value) {
  SlaiStatusRule rule;
  if (value.is_string()) {
    if (value.string() == "open") {
      return rule;
    }
    if (value.string() == "held") {
      rule.kind = SlaiStatusRule::Kind::held;
      return rule;
    }
    throw std::runtime_error("unknown status rule \"" + value.string() + "\"");
  }
  rule.kind = SlaiStatusRule::Kind::flags;
  rule.flags = integer(required(value, "flags"));
  return rule;
}

SlaiRumbleLadder ladder_from(const JsonValue &value) {
  SlaiRumbleLadder ladder;
  ladder.tutorial_record = integer(required(value, "tutorial_record"));
  ladder.ranker_records = member(value, "ranker_records", range_of);
  ladder.rankers_needed = integer(required(value, "rankers_needed"));
  ladder.regional_records = member(value, "regional_records", range_of);
  ladder.regionals_needed = integer(required(value, "regionals_needed"));
  ladder.finals_records = member(value, "finals_records", range_of);
  ladder.finals_needed = integer(required(value, "finals_needed"));
  return ladder;
}

SlaiSpecialValues special_from(const JsonValue &value) {
  SlaiSpecialValues special;
  special.close_range_classes = member(value, "close_range_classes", integers);
  special.lock_on_kinds = member(value, "lock_on_kinds", integers);
  special.grenade_classes = member(value, "grenade_classes", integers);
  special.e_chips = member(value, "e_chips", integers);
  return special;
}

SlaiRumbleBalance balance_from(const JsonValue &value) {
  SlaiRumbleBalance balance;
  balance.entry_scale = value.number_or("entry_scale", balance.entry_scale);
  balance.charge_entry = value.boolean_or("charge_entry", balance.charge_entry);
  balance.prize_scale = value.number_or("prize_scale", balance.prize_scale);
  balance.robots_max = static_cast<int>(value.number_or("robots_max", balance.robots_max));
  if (balance.entry_scale < 0.0 || balance.prize_scale < 0.0 || balance.robots_max < 1) {
    throw std::runtime_error("scales must not be negative and robots_max must be at least 1");
  }
  return balance;
}

} // namespace

SlaiRumbleWorld parse_slai_rumble_world(const JsonValue &document) {
  SlaiRumbleWorld world;
  for (const auto &value : required(document, "cities").array()) {
    SlaiRumbleCity city;
    city.name = required(value, "name").string();
    city.day_stage = required(value, "day_stage").string();
    city.night_stage = required(value, "night_stage").string();
    city.night_lighting = value.string_or("night_lighting", "");
    world.cities.push_back(std::move(city));
  }
  world.start_city = static_cast<int>(document.number_or("start_city", 0));
  if (world.start_city < 0 || world.start_city >= static_cast<int>(world.cities.size())) {
    throw std::runtime_error("start_city names no city");
  }
  for (const auto &maker : required(document, "makers").array()) {
    world.makers.push_back(maker.string());
  }
  for (const auto &[name, types] : required(document, "leg_classes").object()) {
    world.leg_classes.emplace_back(name, integers(types));
  }
  world.special = member(document, "special", special_from);
  for (const auto &rule : required(document, "status_rules").array()) {
    world.status_rules.push_back(status_rule_from(rule));
  }
  world.ladder = member(document, "ladder", ladder_from);
  if (const auto *balance = document.find("balance")) {
    world.balance = balance_from(*balance);
  }
  return world;
}

std::optional<SlaiRumbleData> load_slai_rumble(const DataRoots &roots) {
  const auto table = roots.find_asset(slai_rumble_table_file);
  if (!table) {
    return std::nullopt;
  }
  SlaiRumbleData data;
  try {
    data.table = parse_slai_rumble_table(json::read_file(*table));
  } catch (const std::exception &error) {
    throw std::runtime_error(std::string(slai_rumble_table_file) + ": " + error.what());
  }
  try {
    data.world = parse_slai_rumble_world(roots.document(slai_rumble_world_file));
  } catch (const std::exception &error) {
    throw std::runtime_error(std::string(slai_rumble_world_file) + ": " + error.what());
  }
  if (data.world.cities.size() != data.table.cities.size()) {
    throw std::runtime_error(std::string(slai_rumble_world_file) +
                             ": needs a stage pair for each of the table's cities");
  }
  return data;
}

const std::string &slai_match_stage(const SlaiRumbleWorld &world, const int city,
                                    const bool night) {
  const auto &entry = world.cities.at(static_cast<std::size_t>(city));
  return night ? entry.night_stage : entry.day_stage;
}

} // namespace phantomcrash
