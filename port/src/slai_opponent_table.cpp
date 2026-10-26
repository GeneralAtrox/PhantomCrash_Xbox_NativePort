// Parsing of tables/slai_rumble.json "opponents" (slai/opponent_table.py).

#include "phantomcrash/slai_opponent_table.hpp"

#include "phantomcrash/json_fields.hpp"

#include <algorithm>
#include <stdexcept>
#include <string_view>

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

std::optional<SlaiWeaponCheck> weapon_from(const JsonValue &value) {
  if (value.is_null()) {
    return std::nullopt;
  }
  return SlaiWeaponCheck{integer(required(value, "class")), integer(required(value, "aim"))};
}

std::array<std::optional<SlaiWeaponCheck>, 2> weapons_from(const JsonValue &value) {
  const auto &items = value.array();
  if (items.size() != 2) {
    throw std::runtime_error("needs two weapon slots");
  }
  return {weapon_from(items[0]), weapon_from(items[1])};
}

SlaiOpponentCheck check_from(const JsonValue &value) {
  SlaiOpponentCheck check;
  check.maker = integer(required(value, "maker"));
  check.leg_type = integer(required(value, "leg_type"));
  check.arms = member(value, "arms", weapons_from);
  check.shoulders = member(value, "shoulders", weapons_from);
  check.option_points = integer(required(value, "option_points"));
  check.cpu = integer(required(value, "cpu"));
  return check;
}

SlaiOpponent opponent_from(const JsonValue &value) {
  SlaiOpponent opponent;
  opponent.id = integer(required(value, "id"));
  try {
    opponent.pilot = value.string_or("pilot", "");
    opponent.second_name = value.string_or("second_name", "");
    opponent.level = integer(required(value, "level"));
    opponent.behaviour = integer(required(value, "behaviour"));
    opponent.robot = member(value, "robot", parse_robot_record);
    opponent.check = member(value, "check", check_from);
  } catch (const std::exception &error) {
    throw std::runtime_error("opponent " + std::to_string(opponent.id) + ": " + error.what());
  }
  return opponent;
}

SlaiRoster roster_from(const JsonValue &value) {
  SlaiRoster roster;
  roster.leader = integer(required(value, "leader"));
  const std::vector<int> members = member(value, "members", integers);
  if (members.size() != slai_roster_members) {
    throw std::runtime_error("a roster needs 14 members");
  }
  std::copy(members.begin(), members.end(), roster.members.begin());
  return roster;
}

SlaiSubstitution substitution_from(const JsonValue &value) {
  return {integer(required(value, "replacement")), member(value, "ids", integers),
          member(value, "while_set", integers), member(value, "while_clear", integers)};
}

SlaiLeaderRule leader_rule_from(const JsonValue &value) {
  const std::string_view name = value.string();
  if (name == "flags") {
    return SlaiLeaderRule::flags;
  }
  if (name == "counter") {
    return SlaiLeaderRule::counter;
  }
  if (name == "never") {
    return SlaiLeaderRule::never;
  }
  throw std::runtime_error("leader_rules: unknown rule");
}

} // namespace

const SlaiOpponent *SlaiOpponentTable::opponent(const int id) const noexcept {
  const auto found = std::ranges::lower_bound(opponents, id, {}, &SlaiOpponent::id);
  return found != opponents.end() && found->id == id ? &*found : nullptr;
}

const SlaiRoster *SlaiOpponentTable::roster_of(const int record) const noexcept {
  int index = record;
  for (const auto &[from, roster] : record_rosters) {
    if (from == record) {
      index = roster;
    }
  }
  return index >= 0 && index < static_cast<int>(rosters.size())
             ? &rosters[static_cast<std::size_t>(index)]
             : nullptr;
}

SlaiLeaderRule SlaiOpponentTable::leader_rule(const int regulation) const noexcept {
  return regulation >= 0 && regulation < static_cast<int>(leader_rules.size())
             ? leader_rules[static_cast<std::size_t>(regulation)]
             : SlaiLeaderRule::never;
}

SlaiOpponentTable parse_slai_opponents(const JsonValue &value) {
  SlaiOpponentTable table;
  for (const auto &row : required(value, "rosters").array()) {
    table.rosters.push_back(roster_from(row));
  }
  for (const auto &row : required(value, "record_rosters").array()) {
    table.record_rosters.emplace_back(integer(required(row, "record")),
                                      integer(required(row, "roster")));
  }
  for (const auto &row : required(value, "substitutions").array()) {
    table.substitutions.push_back(substitution_from(row));
  }
  for (const auto &rule : required(value, "leader_rules").array()) {
    table.leader_rules.push_back(leader_rule_from(rule));
  }
  for (const auto &row : required(value, "opponents").array()) {
    table.opponents.push_back(opponent_from(row));
  }
  std::ranges::sort(table.opponents, {}, &SlaiOpponent::id);
  if (std::ranges::adjacent_find(table.opponents, {}, &SlaiOpponent::id) != table.opponents.end()) {
    throw std::runtime_error("an opponent id is listed twice");
  }
  return table;
}

} // namespace phantomcrash
