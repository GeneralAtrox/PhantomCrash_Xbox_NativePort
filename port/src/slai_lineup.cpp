// S.L.A.I. Rumble's line-ups: the opponent check, the candidate lists and
// the per-place draw (slai_lineup.hpp; opponent_rules.py).

#include "phantomcrash/slai_lineup.hpp"

#include <algorithm>
#include <limits>
#include <utility>

namespace phantomcrash {
namespace {

bool contains(const std::vector<int> &values, const int value) {
  return std::find(values.begin(), values.end(), value) != values.end();
}

std::optional<SlaiWeapon> weapon_of(const std::optional<SlaiWeaponCheck> &check) {
  if (!check) {
    return std::nullopt;
  }
  return SlaiWeapon{check->weapon_class, check->aim_kind};
}

// The opponent as the player's entry check reads a robot: ready, its
// options' camouflage, its CPU row standing for the chip.
SlaiRobot robot_of(const SlaiOpponentCheck &check) {
  SlaiRobot robot;
  robot.ready = true;
  robot.maker = check.maker;
  robot.leg_type = check.leg_type;
  robot.optical_camouflage = check.option_points > 0;
  robot.arms = {weapon_of(check.arms[0]), weapon_of(check.arms[1])};
  robot.shoulders = {weapon_of(check.shoulders[0]), weapon_of(check.shoulders[1])};
  robot.chip = check.cpu;
  return robot;
}

SlaiRumbleRecord record_of(const SlaiRumbleSave &save, const int record) {
  const auto found = save.records.find(record);
  return found != save.records.end() ? found->second : SlaiRumbleRecord{};
}

const SlaiOpponent *eligible(const SlaiRumbleData &data, const SlaiRegulation &regulation,
                             const int id, const std::vector<int> &flags) {
  if (id < 0) {
    return nullptr;
  }
  const SlaiOpponentTable &table = data.table.opponents;
  const SlaiOpponent *opponent = table.opponent(slai_substitute(table, id, flags));
  return opponent != nullptr && slai_opponent_eligible(data.world, regulation, *opponent) ? opponent
                                                                                          : nullptr;
}

std::size_t pick(const std::size_t count, const float share) {
  const std::size_t window = std::min(count, slai_pick_window);
  const auto index = static_cast<std::size_t>(std::max(share, 0.0F) * static_cast<float>(window));
  return std::min(index, count - 1);
}

} // namespace

int slai_substitute(const SlaiOpponentTable &table, const int id, const std::vector<int> &flags) {
  for (const SlaiSubstitution &row : table.substitutions) {
    if (!contains(row.ids, id)) {
      continue;
    }
    const bool set = std::ranges::all_of(row.while_set, [&](int f) { return contains(flags, f); });
    const bool clear =
        std::ranges::none_of(row.while_clear, [&](int f) { return contains(flags, f); });
    return set && clear ? row.replacement : id;
  }
  return id;
}

bool slai_opponent_eligible(const SlaiRumbleWorld &world, const SlaiRegulation &regulation,
                            const SlaiOpponent &opponent) {
  if (contains(world.opponents.always_eligible_makers, opponent.check.maker)) {
    return true;
  }
  SlaiRegulation rule = regulation;
  if (rule.special == SlaiSpecialRule::rental_only) {
    rule.special = SlaiSpecialRule::none;
  }
  return slai_entry_check(world, rule, std::numeric_limits<std::int64_t>::max(),
                          robot_of(opponent.check)) == SlaiEntry::allowed;
}

bool slai_leader_ready(const SlaiRumbleData &data, const int regulation,
                       const SlaiRecordStatus status) {
  if (status == SlaiRecordStatus::leader_open) {
    return true;
  }
  switch (data.table.opponents.leader_rule(regulation)) {
  case SlaiLeaderRule::flags:
    return status == SlaiRecordStatus::members_beaten && data.world.opponents.leader_session;
  case SlaiLeaderRule::counter:
    return data.world.opponents.leader_counter;
  case SlaiLeaderRule::never:
    return false;
  }
  return false;
}

SlaiCandidates slai_candidates(const SlaiRumbleData &data, const SlaiMatch &match,
                               const SlaiRumbleSave &save, const std::vector<int> &story_flags) {
  SlaiCandidates found;
  std::vector<int> flags = data.world.opponents.story_flags;
  flags.insert(flags.end(), story_flags.begin(), story_flags.end());
  const SlaiRegulation *regulation = slai_regulation(data.table, match.regulation);
  if (regulation == nullptr) {
    return found;
  }
  const SlaiOpponentTable &table = data.table.opponents;
  constexpr int regulation_own_cameos = 5;
  const std::array<int, 2> records{match.save_record, match.save_record_b};
  for (std::size_t second = 0; second < records.size(); ++second) {
    const int id = records[second];
    const SlaiRoster *roster = id >= 0 ? table.roster_of(id) : nullptr;
    if (roster == nullptr) {
      continue;
    }
    const SlaiRumbleRecord record = record_of(save, id);
    if (second == 0) {
      found.status = slai_record_status(data.world, match.regulation, record);
    }
    for (std::size_t member = 0; member < roster->members.size(); ++member) {
      const SlaiOpponent *opponent = eligible(data, *regulation, roster->members[member], flags);
      if (opponent == nullptr) {
        continue;
      }
      const SlaiCandidate entry{opponent->id, id, static_cast<int>(member) + 1};
      const SlaiList list = second != 0 && match.regulation != regulation_own_cameos
                                ? SlaiList::cameo
                            : record.beaten[member] ? SlaiList::beaten
                                                    : SlaiList::unbeaten;
      found[list].push_back(entry);
    }
    if (second != 0) {
      continue;
    }
    const SlaiOpponent *leader = eligible(data, *regulation, roster->leader, flags);
    if (leader != nullptr && slai_leader_ready(data, match.regulation, found.status)) {
      found[record.cleared ? SlaiList::beaten : SlaiList::leader].push_back(
          {leader->id, id, slai_leader_flag});
    }
  }
  const SlaiRoster *pool = match.pool >= 0 && match.pool < static_cast<int>(table.rosters.size())
                               ? &table.rosters[static_cast<std::size_t>(match.pool)]
                               : nullptr;
  for (std::size_t member = 0; pool != nullptr && member < pool->members.size(); ++member) {
    if (const SlaiOpponent *opponent = eligible(data, *regulation, pool->members[member], flags)) {
      found[SlaiList::pool].push_back({opponent->id, match.pool, static_cast<int>(member) + 1});
    }
  }
  return found;
}

SlaiLineupDraw::SlaiLineupDraw(const SlaiRumbleData &data, const SlaiMatch &match,
                               SlaiRumbleSave save, std::vector<int> story_flags)
    : data_(data), match_(match), save_(std::move(save)), story_flags_(std::move(story_flags)) {
  const auto [low, high] = slai_match_robots(data.world, match);
  robots_min_ = low;
  // robots_max counts the player's robot (0x222070).
  opponents_max_ = std::max(high - 1, 0);
}

std::optional<SlaiDrawn> SlaiLineupDraw::draw(const int place,
                                              const std::function<float()> &share) {
  if (place < 1 || place > slai_battle_places || out_[static_cast<std::size_t>(place)]) {
    return std::nullopt;
  }
  std::array<int, slai_lists> taken{};
  for (const auto &drawn : out_) {
    if (drawn) {
      ++taken[static_cast<std::size_t>(drawn->list)];
    }
  }
  const int pool_taken = taken[static_cast<std::size_t>(SlaiList::pool)];
  const int roster_taken = taken[0] + taken[1] + taken[2] + taken[3];
  if (roster_taken + pool_taken >= opponents_max_) {
    return std::nullopt;
  }
  // 0x223420 takes each drawn entry out of its list.
  SlaiCandidates found = slai_candidates(data_, match_, save_, story_flags_);
  for (auto &list : found.lists) {
    std::erase_if(list, [&](const SlaiCandidate &entry) {
      return std::ranges::any_of(out_, [&](const std::optional<SlaiDrawn> &drawn) {
        return drawn && drawn->candidate.record == entry.record &&
               drawn->candidate.flag == entry.flag;
      });
    });
  }
  const auto has = [&](SlaiList list) { return !found[list].empty(); };
  std::optional<SlaiList> source;
  if (place < slai_first_pool_place) {
    const bool roster_open = roster_taken < slai_roster_limit && roster_taken < robots_min_;
    if (!roster_open) {
      source = std::nullopt;
    } else if (has(SlaiList::leader)) {
      source = SlaiList::leader;
    } else if (has(SlaiList::cameo) && share() < slai_cameo_chance) {
      source = SlaiList::cameo;
    } else if (has(SlaiList::unbeaten)) {
      source =
          found[SlaiList::beaten].size() < slai_beaten_threshold || share() < slai_unbeaten_chance
              ? SlaiList::unbeaten
              : SlaiList::beaten;
    } else if (has(SlaiList::beaten)) {
      source = SlaiList::beaten;
    }
  } else if (pool_taken < slai_pool_limit && has(SlaiList::pool)) {
    source = SlaiList::pool;
  }
  if (!source) {
    return std::nullopt;
  }
  auto &list = found[*source];
  const SlaiDrawn drawn{list[pick(list.size(), share())], *source};
  out_[static_cast<std::size_t>(place)] = drawn;
  return drawn;
}

void SlaiLineupDraw::leave(const int place, const bool by_player) {
  if (place < 1 || place > slai_battle_places) {
    return;
  }
  auto &drawn = out_[static_cast<std::size_t>(place)];
  if (!drawn) {
    return;
  }
  if (by_player && (drawn->list != SlaiList::pool || data_.world.opponents.pool_flags)) {
    if (slai_record_beat(save_, drawn->candidate.record, drawn->candidate.flag)) {
      beaten_.emplace_back(drawn->candidate.record, drawn->candidate.flag);
    }
  }
  drawn.reset();
}

} // namespace phantomcrash
