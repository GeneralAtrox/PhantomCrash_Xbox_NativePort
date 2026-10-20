// S.L.A.I. Rumble's rules: the rota with its finals, the entry check, the
// records, the prize and the stage ladder.

#include "phantomcrash/slai_rumble_rules.hpp"

#include <algorithm>
#include <cmath>

namespace phantomcrash {
namespace {

bool contains(const std::vector<int> &values, const int value) {
  return std::find(values.begin(), values.end(), value) != values.end();
}

const std::vector<int> *leg_types_of(const SlaiRumbleWorld &world, const std::string &name) {
  for (const auto &[leg_class, types] : world.leg_classes) {
    if (leg_class == name) {
      return &types;
    }
  }
  return nullptr;
}

// One of the regulation's leg classes takes the robot's legs; a class the
// world does not name ("any") takes every legs, as does a rule naming none.
bool legs_allowed(const SlaiRumbleWorld &world, const SlaiRegulation &regulation,
                  const int leg_type) {
  if (regulation.leg_classes.empty()) {
    return true;
  }
  for (const auto &name : regulation.leg_classes) {
    const auto *types = leg_types_of(world, name);
    if (types == nullptr || contains(*types, leg_type)) {
      return true;
    }
  }
  return false;
}

template <typename Test> bool every_weapon(const SlaiRobot &robot, Test test) {
  for (const auto *slots : {&robot.arms, &robot.shoulders}) {
    for (const auto &weapon : *slots) {
      if (weapon && !test(*weapon)) {
        return false;
      }
    }
  }
  return true;
}

bool empty(const std::array<std::optional<SlaiWeapon>, 2> &slots) {
  return !slots[0] && !slots[1];
}

bool special_met(const SlaiSpecialValues &values, const SlaiSpecialRule rule,
                 const SlaiRobot &robot) {
  switch (rule) {
  case SlaiSpecialRule::none:
    return true;
  case SlaiSpecialRule::rental_only:
    return false;
  case SlaiSpecialRule::no_optical_camouflage:
    return !robot.optical_camouflage;
  case SlaiSpecialRule::shoulder_weapons_only:
    return empty(robot.arms);
  case SlaiSpecialRule::arm_weapons_only:
    return empty(robot.shoulders);
  case SlaiSpecialRule::close_range_only:
    return every_weapon(robot, [&](const SlaiWeapon &weapon) {
      return contains(values.close_range_classes, weapon.weapon_class);
    });
  case SlaiSpecialRule::lock_on_only:
    return every_weapon(robot, [&](const SlaiWeapon &weapon) {
      return contains(values.lock_on_kinds, weapon.aim_kind);
    });
  case SlaiSpecialRule::grenade_only:
    return every_weapon(robot, [&](const SlaiWeapon &weapon) {
      return contains(values.grenade_classes, weapon.weapon_class);
    });
  case SlaiSpecialRule::e_chip_only:
    return contains(values.e_chips, robot.chip);
  }
  return true;
}

std::int64_t scaled(const std::int64_t value, const double scale) {
  return static_cast<std::int64_t>(std::llround(static_cast<double>(value) * scale));
}

} // namespace

int slai_scheduled_match(const SlaiRumbleTable &table, const int day, const int city,
                         const int stage, const bool night, const int finals_cleared) {
  if (day < 0 || city < 0 || city >= static_cast<int>(table.cities.size()) || stage < 0 ||
      stage >= table.stages) {
    return slai_no_match;
  }
  const auto slot = static_cast<std::size_t>(night ? 1 : 0);
  const auto &row = table.day_rows[static_cast<std::size_t>(day % table.schedule_days)];
  const auto &stages =
      table.city_days[static_cast<std::size_t>(row[static_cast<std::size_t>(city)])];
  const int match = stages[static_cast<std::size_t>(stage)][slot];
  if (stage != table.finals_stage || finals_cleared < 0 ||
      finals_cleared >= static_cast<int>(table.finals.size())) {
    return match;
  }
  const auto &round = table.finals[static_cast<std::size_t>(finals_cleared)];
  if (city == round.host && (round.host_ignores_rota || match >= 0)) {
    return round.matches[slot];
  }
  if (match < 0) {
    return match;
  }
  for (const auto &rule : round.replace) {
    if (contains(rule.from, match)) {
      const auto &pair =
          rule.with ? *rule.with : table.finals_fallback[static_cast<std::size_t>(city)];
      return pair[slot];
    }
  }
  return match;
}

std::array<int, 2> slai_todays_matches(const SlaiRumbleTable &table, const SlaiRumbleSave &save) {
  return {slai_scheduled_match(table, save.day, save.city, save.stage, false, save.finals_cleared),
          slai_scheduled_match(table, save.day, save.city, save.stage, true, save.finals_cleared)};
}

bool slai_record_cleared(const SlaiRumbleSave &save, const int record) {
  const auto found = save.records.find(record);
  return found != save.records.end() && found->second.cleared;
}

int slai_records_cleared(const SlaiRumbleSave &save, const std::array<int, 2> &records) {
  int count = 0;
  for (int record = records[0]; record < records[1]; ++record) {
    count += slai_record_cleared(save, record) ? 1 : 0;
  }
  return count;
}

int slai_next_stage(const SlaiRumbleSave &save, const SlaiRumbleLadder &ladder) {
  switch (save.stage) {
  case 0:
    return slai_record_cleared(save, ladder.tutorial_record) ? 1 : 0;
  case 1:
    return slai_records_cleared(save, ladder.ranker_records) >= ladder.rankers_needed ? 2 : 1;
  case 2:
    return slai_records_cleared(save, ladder.regional_records) >= ladder.regionals_needed ? 3 : 2;
  case 3:
    return slai_records_cleared(save, ladder.finals_records) >= ladder.finals_needed ? 4 : 3;
  default:
    return save.stage;
  }
}

std::int64_t slai_entry_requirement(const SlaiRumbleWorld &world,
                                    const SlaiRegulation &regulation) {
  return scaled(regulation.entry_requirement, world.balance.entry_scale);
}

SlaiEntry slai_entry_check(const SlaiRumbleWorld &world, const SlaiRegulation &regulation,
                           const std::int64_t money, const SlaiRobot &robot) {
  if (money < slai_entry_requirement(world, regulation)) {
    return SlaiEntry::money;
  }
  if (!robot.ready) {
    return SlaiEntry::not_ready;
  }
  if (regulation.maker != slai_any_maker && regulation.maker != robot.maker) {
    return SlaiEntry::maker;
  }
  if (!legs_allowed(world, regulation, robot.leg_type)) {
    return SlaiEntry::legs;
  }
  if (!special_met(world.special, regulation.special, robot)) {
    return SlaiEntry::special;
  }
  return SlaiEntry::allowed;
}

bool slai_pay_entry(QuestSave &quest, const SlaiRumbleWorld &world,
                    const SlaiRegulation &regulation) {
  const std::int64_t requirement = slai_entry_requirement(world, regulation);
  if (quest.money < requirement) {
    return false;
  }
  if (world.balance.charge_entry) {
    quest.money -= requirement;
  }
  return true;
}

std::pair<int, int> slai_match_robots(const SlaiRumbleWorld &world, const SlaiMatch &match) {
  const int cap = world.balance.robots_max;
  return {std::min(match.robots_min, cap), std::min(match.robots_max, cap)};
}

SlaiRecordStatus slai_record_status(const SlaiRumbleWorld &world, const int regulation,
                                    const SlaiRumbleRecord &record) {
  const SlaiStatusRule rule =
      regulation >= 0 && regulation < static_cast<int>(world.status_rules.size())
          ? world.status_rules[static_cast<std::size_t>(regulation)]
          : SlaiStatusRule{};
  switch (rule.kind) {
  case SlaiStatusRule::Kind::open:
    return SlaiRecordStatus::leader_open;
  case SlaiStatusRule::Kind::held:
    return SlaiRecordStatus::members_beaten;
  case SlaiStatusRule::Kind::flags:
    break;
  }
  if (record.cleared) {
    return SlaiRecordStatus::leader_open;
  }
  const auto flags =
      std::min(static_cast<std::size_t>(std::max(rule.flags, 0)), record.beaten.size());
  const bool all =
      std::all_of(record.beaten.begin(), record.beaten.begin() + static_cast<std::ptrdiff_t>(flags),
                  [](const bool beaten) { return beaten; });
  return all ? SlaiRecordStatus::members_beaten : SlaiRecordStatus::open_members;
}

bool slai_record_beat(SlaiRumbleSave &save, const int record, const int flag) {
  if (record < 0 || flag < slai_leader_flag || flag > static_cast<int>(slai_record_flags)) {
    return false;
  }
  auto &entry = save.records[record];
  bool &mark =
      flag == slai_leader_flag ? entry.cleared : entry.beaten[static_cast<std::size_t>(flag - 1)];
  if (mark) {
    return false;
  }
  mark = true;
  return true;
}

std::int64_t slai_prize(const SlaiRumbleWorld &world, const SlaiRegulation &regulation,
                        const int score) {
  if (regulation.target < 0 || score < regulation.target) {
    return 0;
  }
  return scaled(regulation.prize, world.balance.prize_scale);
}

SlaiMatchOutcome slai_finish_match(QuestSave &quest, const SlaiRumbleData &data,
                                   const SlaiMatchResult &result) {
  SlaiRumbleSave &save = quest.slai_rumble;
  SlaiMatchOutcome outcome;
  outcome.stage_before = save.stage;
  const SlaiMatch *match = slai_match(data.table, result.match);
  const bool was_cleared = match != nullptr && slai_record_cleared(save, match->save_record);
  for (const auto &[record, flag] : result.beaten) {
    slai_record_beat(save, record, flag);
  }
  save.finals_cleared = slai_records_cleared(save, data.world.ladder.finals_records);
  if (match != nullptr) {
    outcome.cleared = !was_cleared && slai_record_cleared(save, match->save_record);
    outcome.prize =
        slai_prize(data.world, data.table.regulations[static_cast<std::size_t>(match->regulation)],
                   result.score);
    quest.money = std::min(quest.money + outcome.prize, money_limit);
  }
  save.stage = slai_next_stage(save, data.world.ladder);
  outcome.stage_after = save.stage;
  return outcome;
}

void advance_slai_rumble_day(SlaiRumbleSave &save, const int days_in_cycle) {
  if (days_in_cycle <= 0) {
    return;
  }
  save.day = (std::max(save.day, 0) + 1) % days_in_cycle;
}

} // namespace phantomcrash
