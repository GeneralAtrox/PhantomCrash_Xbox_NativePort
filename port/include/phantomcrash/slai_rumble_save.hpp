#pragma once

// S.L.A.I. Rumble's part of a game's save (QuestSave::slai_rumble), as the
// original keeps it in its save object: see docs/SLAI_CAMPAIGN_RESEARCH.md,
// "Save layout". A save from before S.L.A.I. Rumble loads it empty: day 0,
// stage 0, nothing cleared.

#include "phantomcrash/json.hpp"

#include <array>
#include <cstddef>
#include <map>

namespace phantomcrash {

// A record's opponent flags (bytes 1..14); flag 0 is its leader, whose
// defeat clears the record (byte 0).
inline constexpr std::size_t slai_record_flags = 14;
inline constexpr int slai_leader_flag = 0;

// A 15-byte rumble record (save + id x 15).
struct SlaiRumbleRecord {
  bool cleared{};
  std::array<bool, slai_record_flags> beaten{}; // flags 1..14

  bool operator==(const SlaiRumbleRecord &) const = default;
};

struct SlaiRumbleSave {
  int day{};            // + 0xa10: the rota day, 0..99
  int stage{};          // + 0xa21: the stage ladder, 0..4
  int city{};           // + 0x4769: where the player is
  int finals_cleared{}; // 0x2574e0: the finals records cleared, 0..3
  // By save record id; a record not here has nothing set.
  std::map<int, SlaiRumbleRecord> records;

  bool operator==(const SlaiRumbleSave &) const = default;
};

// Missing members keep their defaults and negative counts read as 0;
// records with nothing set are left out.
[[nodiscard]] SlaiRumbleSave slai_rumble_from_json(const json::JsonValue &value);
[[nodiscard]] json::JsonValue slai_rumble_to_json(const SlaiRumbleSave &save);

} // namespace phantomcrash
