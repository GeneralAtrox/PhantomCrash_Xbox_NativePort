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

SlaiBattleRules battle_from(const JsonValue &value) {
  SlaiBattleRules battle;
  battle.spawn_route = value.boolean_or("spawn_route", battle.spawn_route);
  battle.spawn_merge = static_cast<float>(value.number_or("spawn_merge", battle.spawn_merge));
  battle.unstick_seconds =
      static_cast<float>(value.number_or("unstick_seconds", battle.unstick_seconds));
  battle.hop_seconds = static_cast<float>(value.number_or("hop_seconds", battle.hop_seconds));
  battle.hops = static_cast<int>(value.number_or("hops", battle.hops));
  if (battle.unstick_seconds < 0.0F || battle.hop_seconds < 0.0F || battle.hops < 0) {
    throw std::runtime_error("unstick_seconds, hop_seconds and hops must not be negative");
  }
  if (battle.spawn_merge < 0.0F) {
    throw std::runtime_error("spawn_merge must not be negative");
  }
  return battle;
}

SlaiOpponentRules opponent_rules_from(const JsonValue &value) {
  SlaiOpponentRules rules;
  if (const auto *makers = value.find("always_eligible_makers")) {
    rules.always_eligible_makers = integers(*makers);
  }
  if (const auto *flags = value.find("story_flags")) {
    rules.story_flags = integers(*flags);
  }
  rules.leader_session = value.boolean_or("leader_session", rules.leader_session);
  rules.leader_counter = value.boolean_or("leader_counter", rules.leader_counter);
  if (const auto *chips = value.find("chips")) {
    for (const auto &row : chips->array()) {
      rules.chips.push_back({integer(required(row, "from_level")), integer(required(row, "chip")),
                             static_cast<int>(row.number_or("chip_level", 0))});
    }
  }
  if (const auto *profiles = value.find("profiles")) {
    for (const auto &[behaviour, profile] : profiles->object()) {
      rules.profiles.emplace_back(std::stoi(behaviour), integer(profile));
    }
  }
  rules.profile = static_cast<int>(value.number_or("profile", rules.profile));
  rules.paint = static_cast<int>(value.number_or("paint", rules.paint));
  rules.emblem = static_cast<int>(value.number_or("emblem", rules.emblem));
  rules.arrival_seconds =
      static_cast<float>(value.number_or("arrival_seconds", rules.arrival_seconds));
  rules.pool_flags = value.boolean_or("pool_flags", rules.pool_flags);
  if (rules.arrival_seconds < 0.0F) {
    throw std::runtime_error("opponents.arrival_seconds must not be negative");
  }
  return rules;
}

} // namespace

SlaiRumbleWorld parse_slai_rumble_world(const JsonValue &document) {
  SlaiRumbleWorld world;
  for (const auto &value : required(document, "cities").array()) {
    SlaiRumbleCity city;
    city.name = required(value, "name").string();
    city.day_stage = required(value, "day_stage").string();
    city.night_stage = required(value, "night_stage").string();
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
  if (const auto *battle = document.find("battle")) {
    world.battle = battle_from(*battle);
  }
  if (document.find("opponents") != nullptr) {
    world.opponents = member(document, "opponents", opponent_rules_from);
  }
  if (document.find("entry") != nullptr) {
    world.entry = member(document, "entry", parse_slai_entry_rules);
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
  try {
    data.rota =
        parse_slai_rota(roots.document(slai_rota_file), static_cast<int>(data.table.cities.size()),
                        data.table.schedule_days);
  } catch (const std::exception &error) {
    throw std::runtime_error(std::string(slai_rota_file) + ": " + error.what());
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
