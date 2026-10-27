#pragma once

// S.L.A.I.'s story progress in the shared save (QuestSave::slai_story):
// the original's 256 event flags at save + 0x426F, which the story event
// table's required, blocking and completion flags name, and the counts the
// rows' conditions read (docs/SLAI_STORY_PLAN.md). A save from before the
// story loads with none set.

#include "phantomcrash/json.hpp"

#include <array>
#include <cstddef>
#include <cstdint>
#include <vector>

namespace phantomcrash {

inline constexpr std::size_t slai_story_flags = 256;
// No sponsor (save + 0x4269's and + 0x426A's initial value).
inline constexpr int slai_no_sponsor = 5;

struct SlaiStorySave {
  std::array<bool, slai_story_flags> flags{};
  // save + 0x41DC: the nights slept since the story began (0x4E8CF0, up to
  // 9999). The tutorial's endings tell a first-day clear from a later one.
  int days{};
  // The port's night count (QuestSave::total_days) the days last caught up
  // with; -1 until the story first looks.
  int nights_seen{-1};
  // save + 0x41CC: the enemies the player has shot down in S.L.A.I. matches.
  std::int64_t defeated{};
  // The sponsors: the current and the previous one (+ 0x4269, + 0x426A;
  // slai_no_sponsor for none), the makers that have sponsored (+ 0x426B,
  // bit by maker), the battles shot down in a row (+ 0x426C) and why the
  // last sponsor left (+ 0x426E: 1 body, 2 shot down, 3 score).
  int sponsor{slai_no_sponsor};
  int previous_sponsor{slai_no_sponsor};
  int sponsors_had{};
  int shot_down_streak{};
  int dismissal{};
  // The last battle's result: the player was still standing (global +
  // 0x489D).
  bool survived{};

  bool operator==(const SlaiStorySave &) const = default;
};

// The flags set, ascending (what 0x204e50 finds set).
[[nodiscard]] std::vector<int> slai_story_flags_set(const SlaiStorySave &save);

// Flags are written as the list of those set; out-of-range ones are dropped.
[[nodiscard]] SlaiStorySave slai_story_from_json(const json::JsonValue &value);
[[nodiscard]] json::JsonValue slai_story_to_json(const SlaiStorySave &save);

} // namespace phantomcrash
