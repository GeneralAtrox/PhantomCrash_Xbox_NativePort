// S.L.A.I.'s rumble line-up as the battle fields it (slai_battle_lineup.hpp).

#include "phantomcrash/slai_battle_lineup.hpp"

#include <string>

namespace phantomcrash {
namespace {

// The HUD markers' colours (RosterSource order): roster members, leaders
// and pool drones as RUMBLING's members, leaders and reserves (a port
// choice: S.L.A.I.'s markers are unread).
constexpr int member_marker = 0;
constexpr int leader_marker = 1;
constexpr int pool_marker = 3;

int marker_of(const SlaiList list) {
  switch (list) {
  case SlaiList::leader:
    return leader_marker;
  case SlaiList::pool:
    return pool_marker;
  default:
    return member_marker;
  }
}

} // namespace

const SlaiOpponentChip *slai_opponent_chip(const SlaiOpponentRules &rules, const int level) {
  const SlaiOpponentChip *found = nullptr;
  for (const SlaiOpponentChip &row : rules.chips) {
    if (level >= row.from_level && (found == nullptr || row.from_level >= found->from_level)) {
      found = &row;
    }
  }
  return found;
}

int slai_opponent_profile(const SlaiOpponentRules &rules, const int behaviour) {
  for (const auto &[from, profile] : rules.profiles) {
    if (from == behaviour) {
      return profile;
    }
  }
  return rules.profile;
}

RobotRecord slai_opponent_robot(const SlaiOpponentRules &rules, const SlaiOpponent &opponent) {
  RobotRecord record = opponent.robot;
  if (const SlaiOpponentChip *chip = slai_opponent_chip(rules, opponent.level);
      chip != nullptr && chip->chip >= 0) {
    record.parts[static_cast<std::size_t>(PartSlot::chip)] =
        RobotPart{chip->chip, chip->chip_level};
  }
  if (rules.paint >= 0) {
    record.paint = rules.paint;
  }
  if (rules.emblem >= 0) {
    record.emblem = rules.emblem;
  }
  return record;
}

SlaiBattleLineup::SlaiBattleLineup(const SlaiRumbleData &data, const SlaiMatch &match,
                                   const SlaiRumbleSave &save, const RumblingTables &tables,
                                   const MovementTuning &movement, const unsigned int seed)
    : data_(data), draw_(data, match, save), rules_(tables.rules),
      tables_{tables.parts, tables.garage,    tables.contests, rules_,
              tables.drive, tables.templates, tables.hud},
      movement_(movement), random_(seed) {}

std::optional<LineupRobot> SlaiBattleLineup::enter(const int place) {
  std::uniform_real_distribution<float> share_of(0.0F, 1.0F);
  const auto drawn = draw_.draw(place, [&] { return share_of(random_); });
  const SlaiOpponent *opponent =
      drawn ? data_.table.opponents.opponent(drawn->candidate.opponent) : nullptr;
  if (opponent == nullptr) {
    return std::nullopt;
  }
  const SlaiOpponentRules &rules = data_.world.opponents;
  // S.L.A.I.'s opponents are not raised by a cleared Phantom Crash's laps.
  static const QuestSave unraised{};
  LineupRobot robot;
  robot.robot = record_robot(tables_, unraised, slai_opponent_robot(rules, *opponent),
                             "slai" + std::to_string(place), movement_);
  robot.robot.tag = opponent->pilot;
  robot.ability = opponent->level;
  robot.profile = slai_opponent_profile(rules, opponent->behaviour);
  robot.marker = marker_of(drawn->list);
  return robot;
}

void SlaiBattleLineup::leave(const int place, const bool by_player) {
  draw_.leave(place, by_player);
}

} // namespace phantomcrash
