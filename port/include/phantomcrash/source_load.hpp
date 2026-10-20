#pragma once

// The load factor S.L.A.I. legs turn by (PAL SLES_529.40, 0x2568d0, called
// from 0x4dd5dc). Like the Phantom Crash speed factor it is the square root
// of the frame (body and legs at level 0, 0x256460) over the weight
// (0x254900), trimmed by the legs' level; an overload grades it by the
// excess: (capacity / weight) to n squared for each 35 over (n = 1..3), to
// the 10th from the fourth band on. 0x256700 turns it into the legs' turn
// speed (+0x8c, +0x88): their turn times it, within 0.1..90 degrees.

#include "phantomcrash/json.hpp"

#include <optional>

namespace phantomcrash {

struct SourceLoadRule {
  int level_max{99};           // 0x256934
  float level_trim{0.1F};      // 0x256944
  float overload_band{35.0F};  // 0x256994
  int graded_bands{3};         // 0x2569cc: n < 4 takes n * n
  float overload_power{10.0F}; // 0x2569bc
};

// A legs record's "source_load" (absent for Phantom Crash legs).
[[nodiscard]] std::optional<SourceLoadRule> parse_source_load(const json::JsonValue *value);

[[nodiscard]] float source_load_factor(const SourceLoadRule &rule, float frame, float weight,
                                       float capacity, int legs_level);

// 0x256838..56850: the turn's range, in degrees a vsync.
inline constexpr float source_turn_min = 0.1F;
inline constexpr float source_turn_max = 90.0F;

} // namespace phantomcrash
