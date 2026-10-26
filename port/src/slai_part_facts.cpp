// Parsing of tables/slai_rumble.json "parts" and data/world/slai_rumble.json
// "entry" (slai_part_facts.hpp).

#include "phantomcrash/slai_part_facts.hpp"

#include "phantomcrash/json_fields.hpp"

#include <stdexcept>

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

SlaiWeaponCheck weapon_from(const JsonValue &value) {
  return {integer(required(value, "class")), integer(required(value, "aim"))};
}

// Rows of {"part": id, `key`: value} as a map by part.
std::map<int, int> values_by_part(const JsonValue &rows, const char *key) {
  std::map<int, int> out;
  for (const auto &row : rows.array()) {
    out[integer(required(row, "part"))] = integer(required(row, key));
  }
  return out;
}

std::map<int, SlaiWeaponCheck> weapons_by_part(const JsonValue &rows) {
  std::map<int, SlaiWeaponCheck> out;
  for (const auto &row : rows.array()) {
    out[integer(required(row, "part"))] = weapon_from(row);
  }
  return out;
}

} // namespace

SlaiPartFacts parse_slai_part_facts(const JsonValue &value) {
  SlaiPartFacts facts;
  facts.body_makers = values_by_part(required(value, "bodies"), "maker");
  facts.leg_types = values_by_part(required(value, "legs"), "leg_type");
  facts.arm_weapons = member(value, "arm_weapons", weapons_by_part);
  facts.shoulder_weapons = member(value, "shoulder_weapons", weapons_by_part);
  return facts;
}

SlaiEntryRules parse_slai_entry_rules(const JsonValue &value) {
  SlaiEntryRules rules;
  if (const auto *types = value.find("leg_types")) {
    rules.leg_types = integers(*types);
  }
  if (const auto *weapons = value.find("weapons")) {
    rules.weapons.clear();
    for (const auto &row : weapons->array()) {
      SlaiKindFacts facts;
      facts.kind = integer(required(row, "kind"));
      facts.check.weapon_class = static_cast<int>(row.number_or("class", 0.0));
      facts.check.aim_kind = static_cast<int>(row.number_or("aim", 0.0));
      rules.weapons.push_back(facts);
    }
  }
  if (const auto *chips = value.find("e_chips")) {
    rules.e_chips = integers(*chips);
  }
  if (const auto *rentals = value.find("rental_robots")) {
    rules.rental_robots = integers(*rentals);
  }
  return rules;
}

} // namespace phantomcrash
