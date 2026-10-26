#pragma once

// What S.L.A.I.'s entry check (0x259a40) reads of the port's parts. The
// imported S.L.A.I. parts keep their own values, which tables/slai_rumble.json
// "parts" carries (slai/opponent_table.py part_facts). Phantom Crash's parts
// have none, so data/world/slai_rumble.json "entry" states the port's
// reading of them. The adapter that reads a ridden robot through both is
// slai_entry_robot.hpp.

#include "phantomcrash/json.hpp"
#include "phantomcrash/slai_opponent_table.hpp"

#include <map>
#include <vector>

namespace phantomcrash {

// The imported parts' values, by the port's part id.
struct SlaiPartFacts {
  std::map<int, int> body_makers; // the body record's maker (+2)
  std::map<int, int> leg_types;   // 0x24d7d0 over the legs record's +6
  std::map<int, SlaiWeaponCheck> arm_weapons;
  std::map<int, SlaiWeaponCheck> shoulder_weapons;

  [[nodiscard]] bool empty() const noexcept {
    return body_makers.empty() && leg_types.empty() && arm_weapons.empty() &&
           shoulder_weapons.empty();
  }
};

// A Phantom Crash weapon's S.L.A.I. class and aim kind, by its HUD kind
// (WeaponRecord kind).
struct SlaiKindFacts {
  int kind{};
  SlaiWeaponCheck check;
};

// How Phantom Crash's own parts answer the check (port choices).
struct SlaiEntryRules {
  // The S.L.A.I. leg type of each Phantom Crash legs type (rumbling.json
  // legs_types): 2, 4 and 6 legs, rollers on 2, 3, 4 and 6 wheels, hover.
  std::vector<int> leg_types{0, 1, 2, 3, 4, 5, 6, 7};
  // A HUD kind not listed is class 0 with aim kind 0.
  std::vector<SlaiKindFacts> weapons{{6, {2, 0}}, {3, {3, 0}}, {1, {0, 1}}};
  // The CHIP records that pass as E-Chips (S.L.A.I.'s CPU rows 0 and 1).
  std::vector<int> e_chips{0, 1};
  // The tables/garage.json robots a rental-only rumble lends; empty lends
  // RUMBLING's rentals (rumbling.json rental_robots).
  std::vector<int> rental_robots;
};

// Throw std::runtime_error naming the member at fault.
[[nodiscard]] SlaiPartFacts parse_slai_part_facts(const json::JsonValue &value);
[[nodiscard]] SlaiEntryRules parse_slai_entry_rules(const json::JsonValue &value);

} // namespace phantomcrash
