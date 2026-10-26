#pragma once

// A port robot as S.L.A.I.'s entry check (0x259a40) reads it. Imported
// S.L.A.I. parts answer with their own values (the table's "parts"), and
// Phantom Crash's parts as data/world/slai_rumble.json "entry" reads them
// (slai_part_facts.hpp). A Phantom Crash body has no S.L.A.I. maker, so a
// maker's rumble refuses it.

#include "phantomcrash/rumbling_rules.hpp"
#include "phantomcrash/slai_rumble_rules.hpp"

#include <vector>

namespace phantomcrash {

// A hangar's robot, ready when it has a body, legs and a CHIP. Its optical
// camouflage is its modules' O.C. time (robot_totals active_seconds).
[[nodiscard]] SlaiRobot slai_entry_robot(const SlaiRumbleData &data, const RegulationCheck &check,
                                         const Garage &garage, const Hangar &hangar);
// The ridden robot; not ready when none is ridden (riding_ready).
[[nodiscard]] SlaiRobot slai_riding_robot(const SlaiRumbleData &data, const RegulationCheck &check,
                                          const QuestSave &quest);

// The tables/garage.json robots a rental-only rumble lends: the world's
// entry.rental_robots, else RUMBLING's.
[[nodiscard]] const std::vector<int> &slai_rental_robots(const SlaiRumbleWorld &world,
                                                         const RumblingRules &rules);

} // namespace phantomcrash
