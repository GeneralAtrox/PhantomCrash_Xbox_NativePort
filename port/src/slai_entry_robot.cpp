// A port robot in S.L.A.I.'s terms for its entry check
// (phantomcrash/slai_entry_robot.hpp).

#include "phantomcrash/slai_entry_robot.hpp"

#include "phantomcrash/robot_totals.hpp"

#include <algorithm>

namespace phantomcrash {
namespace {

const StockItem *fitted(const Garage &garage, const Hangar &hangar, const PartSlot slot) {
  const auto found = garage.stock.find(hangar.part(slot));
  return found != garage.stock.end() ? &found->second : nullptr;
}

template <typename Record> const Record *record_at(const std::vector<Record> &records, int id) {
  return id >= 0 && id < static_cast<int>(records.size()) ? &records[static_cast<std::size_t>(id)]
                                                          : nullptr;
}

int value_at(const std::vector<int> &values, const int index, const int fallback) {
  const int *found = record_at(values, index);
  return found != nullptr ? *found : fallback;
}

// The legs' S.L.A.I. leg type: an imported legs' own, else its Phantom
// Crash legs type's as the world reads it; -1 for none.
int leg_type_of(const SlaiRumbleData &data, const RegulationCheck &check, const int id) {
  const auto own = data.table.parts.leg_types.find(id);
  if (own != data.table.parts.leg_types.end()) {
    return own->second;
  }
  const ArmourRecord *legs = record_at(check.parts.legs, id);
  if (legs == nullptr) {
    return -1;
  }
  const int type = value_at(check.rules.legs_types, legs->model, -1);
  return type < 0 ? -1 : value_at(data.world.entry.leg_types, type, -1);
}

SlaiWeaponCheck by_kind(const SlaiEntryRules &rules, const WeaponRecord *weapon) {
  if (weapon != nullptr) {
    for (const SlaiKindFacts &facts : rules.weapons) {
      if (facts.kind == weapon->kind) {
        return facts.check;
      }
    }
  }
  return {};
}

std::optional<SlaiWeapon> weapon_of(const SlaiRumbleData &data, const RegulationCheck &check,
                                    const StockItem *item, const bool shoulder) {
  if (item == nullptr) {
    return std::nullopt;
  }
  const auto &own = shoulder ? data.table.parts.shoulder_weapons : data.table.parts.arm_weapons;
  const auto found = own.find(item->id);
  const SlaiWeaponCheck facts =
      found != own.end()
          ? found->second
          : by_kind(data.world.entry,
                    record_at(shoulder ? check.parts.shoulder_weapons : check.parts.arm_weapons,
                              item->id));
  return SlaiWeapon{facts.weapon_class, facts.aim_kind};
}

bool contains(const std::vector<int> &values, const int value) {
  return std::ranges::find(values, value) != values.end();
}

} // namespace

SlaiRobot slai_entry_robot(const SlaiRumbleData &data, const RegulationCheck &check,
                           const Garage &garage, const Hangar &hangar) {
  SlaiRobot robot;
  robot.ready = hangar.owned && hangar.complete();
  if (const StockItem *body = fitted(garage, hangar, PartSlot::body)) {
    const auto maker = data.table.parts.body_makers.find(body->id);
    robot.maker = maker != data.table.parts.body_makers.end() ? maker->second : slai_any_maker;
  }
  if (const StockItem *legs = fitted(garage, hangar, PartSlot::legs)) {
    robot.leg_type = leg_type_of(data, check, legs->id);
  }
  robot.optical_camouflage =
      robot_totals(check.stats, check.parts, check.garage, garage, hangar).active_seconds > 0.0;
  robot.arms = {weapon_of(data, check, fitted(garage, hangar, PartSlot::right_arm), false),
                weapon_of(data, check, fitted(garage, hangar, PartSlot::left_arm), false)};
  robot.shoulders = {weapon_of(data, check, fitted(garage, hangar, PartSlot::right_shoulder), true),
                     weapon_of(data, check, fitted(garage, hangar, PartSlot::left_shoulder), true)};
  // A CHIP the world counts as an E-Chip stands for S.L.A.I.'s first.
  const StockItem *chip = fitted(garage, hangar, PartSlot::chip);
  const std::vector<int> &e_chips = data.world.special.e_chips;
  if (chip != nullptr && !e_chips.empty() && contains(data.world.entry.e_chips, chip->id)) {
    robot.chip = e_chips.front();
  }
  return robot;
}

SlaiRobot slai_riding_robot(const SlaiRumbleData &data, const RegulationCheck &check,
                            const QuestSave &quest) {
  if (!riding_ready(quest)) {
    return {};
  }
  const Garage &garage = quest.garage;
  return slai_entry_robot(data, check, garage,
                          garage.hangars[static_cast<std::size_t>(garage.riding)]);
}

const std::vector<int> &slai_rental_robots(const SlaiRumbleWorld &world,
                                           const RumblingRules &rules) {
  return world.entry.rental_robots.empty() ? rules.rental_robots : world.entry.rental_robots;
}

} // namespace phantomcrash
