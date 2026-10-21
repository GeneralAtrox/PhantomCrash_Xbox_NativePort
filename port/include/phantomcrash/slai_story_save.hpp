#pragma once

// S.L.A.I.'s story progress in the shared save (QuestSave::slai_story):
// the original's 256 event flags at save + 0x426F, which the story event
// table's required, blocking and completion flags name (docs/
// SLAI_STORY_PLAN.md). A save from before the story loads with none set.

#include "phantomcrash/json.hpp"

#include <array>
#include <cstddef>

namespace phantomcrash {

inline constexpr std::size_t slai_story_flags = 256;

struct SlaiStorySave {
  std::array<bool, slai_story_flags> flags{};
  // save + 0x4448, which the tutorial's two endings test (bs03k without it,
  // bs04k with it); what sets it is not yet known.
  bool tutorial_mark{};

  bool operator==(const SlaiStorySave &) const = default;
};

// Flags are written as the list of those set; out-of-range ones are dropped.
[[nodiscard]] SlaiStorySave slai_story_from_json(const json::JsonValue &value);
[[nodiscard]] json::JsonValue slai_story_to_json(const SlaiStorySave &save);

} // namespace phantomcrash
