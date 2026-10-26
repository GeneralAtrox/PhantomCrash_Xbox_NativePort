#pragma once

// Who S.L.A.I.'s rumbles field against the player: the original's opponent
// check (0x222a60), its candidate lists (0x2225a0) and its per-slot draw
// (0x223420), over the opponents tables/slai_rumble.json carries
// (slai_opponent_table.hpp) and the save's records. opponent_rules.py
// mirrors the same code and gives the tests their expected values. PAL
// SLES-52940 addresses; see docs/SLAI_OPPONENTS_RESEARCH.md.

#include "phantomcrash/slai_rumble_rules.hpp"

#include <array>
#include <cstddef>
#include <functional>
#include <optional>
#include <vector>

namespace phantomcrash {

// 0x256cc0: the opponent standing in for `id` under the story flags set.
[[nodiscard]] int slai_substitute(const SlaiOpponentTable &table, int id,
                                  const std::vector<int> &flags);

// 0x222a60: an opponent may take part in a regulation's rumble. The
// rental-only rule binds only the player.
[[nodiscard]] bool slai_opponent_eligible(const SlaiRumbleWorld &world,
                                          const SlaiRegulation &regulation,
                                          const SlaiOpponent &opponent);

// 0x222f70: a record's leader may join at this status.
[[nodiscard]] bool slai_leader_ready(const SlaiRumbleData &data, int regulation,
                                     SlaiRecordStatus status);

// 0x2225a0's lists, in its order.
enum class SlaiList { unbeaten, cameo, leader, beaten, pool };
inline constexpr std::size_t slai_lists = 5;

// An opponent a list offers: its save record and flag there (0 the leader;
// a pool drone's record is its pool roster).
struct SlaiCandidate {
  int opponent{slai_no_opponent};
  int record{-1};
  int flag{};
  bool operator==(const SlaiCandidate &) const = default;
};

struct SlaiCandidates {
  std::array<std::vector<SlaiCandidate>, slai_lists> lists;
  SlaiRecordStatus status{SlaiRecordStatus::open_members};

  [[nodiscard]] std::vector<SlaiCandidate> &operator[](SlaiList list) {
    return lists[static_cast<std::size_t>(list)];
  }
  [[nodiscard]] const std::vector<SlaiCandidate> &operator[](SlaiList list) const {
    return lists[static_cast<std::size_t>(list)];
  }
};

// 0x2225a0 (constructor arguments 0, 0): the match's record roster sorted
// into unbeaten and beaten, its second record's into cameos (regulation 5
// sorts those as its own), the leader once ready, and the pool roster.
[[nodiscard]] SlaiCandidates slai_candidates(const SlaiRumbleData &data, const SlaiMatch &match,
                                             const SlaiRumbleSave &save);

// The draw's limits (0x223420).
inline constexpr int slai_first_pool_place = 4; // places below draw the rosters
inline constexpr int slai_roster_limit = 4;     // roster opponents out at once
inline constexpr int slai_pool_limit = 2;       // pool drones out at once
inline constexpr std::size_t slai_pick_window = 3;
inline constexpr std::size_t slai_beaten_threshold = 4;
inline constexpr float slai_cameo_chance = 1.0F / 64.0F; // 0x3c800000
inline constexpr float slai_unbeaten_chance = 0.95F;     // 0x3f733333
inline constexpr int slai_battle_places = 5;             // 1..5; 0 is the player

// One opponent drawn into a place.
struct SlaiDrawn {
  SlaiCandidate candidate;
  SlaiList list{};
};

// A match's line-up as the battle goes (0x2232e0, 0x223420): place by
// place, each from a fresh 0x2225a0 over the records as this battle has
// filled them (0x223ed0 re-checks after each defeat) less the opponents out.
// Places 1..3 draw the rosters while fewer than robots_min (and 4) of theirs
// are out; places 4..5 the pool, two at most. A place emptied by a
// shootdown draws again (a port reading: the rumbles' 20 to 30 shootdown
// targets need it).
class SlaiLineupDraw {
public:
  SlaiLineupDraw(const SlaiRumbleData &data, const SlaiMatch &match, SlaiRumbleSave save);

  // The opponent entering `place` (1..slai_battle_places), or none. `share`
  // stands for 0x249c50's rand() / 32768, in [0, 1).
  [[nodiscard]] std::optional<SlaiDrawn> draw(int place, const std::function<float()> &share);
  // Its opponent left `place`; one the player shot down is beaten.
  void leave(int place, bool by_player);

  // The (record, flag) pairs the player beat, in order (pool drones only
  // when the world's pool_flags is set).
  [[nodiscard]] const std::vector<std::pair<int, int>> &beaten() const noexcept {
    return beaten_;
  }

private:
  const SlaiRumbleData &data_;
  const SlaiMatch &match_;
  SlaiRumbleSave save_;
  int robots_min_{};
  int opponents_max_{};
  std::array<std::optional<SlaiDrawn>, slai_battle_places + 1> out_{};
  std::vector<std::pair<int, int>> beaten_;
};

} // namespace phantomcrash
