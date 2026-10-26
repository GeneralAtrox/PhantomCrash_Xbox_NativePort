#pragma once

// S.L.A.I. Rumble's opponents as tables/slai_rumble.json "opponents" carries
// them (slai/opponent_table.py): the campaign's rosters and every opponent
// they name, its robot already on the port's imported parts. See
// docs/SLAI_OPPONENTS_RESEARCH.md; addresses are PAL SLES-52940 and names
// researcher-assigned. The rules that field them are slai_lineup.hpp.

#include "phantomcrash/garage_tables.hpp"
#include "phantomcrash/json.hpp"

#include <array>
#include <optional>
#include <string>
#include <utility>
#include <vector>

namespace phantomcrash {

inline constexpr int slai_no_opponent = -1;
inline constexpr std::size_t slai_roster_members = 14;

// A weapon as the opponent check reads it (0x222a60).
struct SlaiWeaponCheck {
  int weapon_class{}; // +0x134: 2 close range, 3 or 15 grenade
  int aim_kind{};     // +0x18: 1 or 9 lock-on
};

// What the opponent check reads of an opponent's build.
struct SlaiOpponentCheck {
  int maker{};    // the body's maker (+2); 8 and 9 are the drones'
  int leg_type{}; // 0x24d7d0, 0..10; 11 without legs
  std::array<std::optional<SlaiWeaponCheck>, 2> arms;
  std::array<std::optional<SlaiWeaponCheck>, 2> shoulders;
  int option_points{}; // its options' +0x1a total: above 0, optical camouflage
  int cpu{-1};         // the CPU chip row (0, 1: E-Chips)
};

// An opponent row (0x37e600, 8 bytes): a pilot on a robot at a level.
struct SlaiOpponent {
  int id{};
  std::string pilot;       // CHARANAME text
  std::string second_name; // the name reference's second name (its robot)
  int level{};             // +6, entrant +0xbc
  int behaviour{};         // +4, entrant +0xbe; what it selects is unresolved
  RobotRecord robot;       // on the port's parts; no CHIP (the CPU row is undecoded)
  SlaiOpponentCheck check;
};

// A roster row (0x420f40): its leader, then the members whose beaten flags
// are a save record's bytes 1..14; slai_no_opponent for none.
struct SlaiRoster {
  int leader{slai_no_opponent};
  std::array<int, slai_roster_members> members{};
};

// 0x256cc0: the first row listing an id replaces it while its story flags
// hold (set, clear).
struct SlaiSubstitution {
  int replacement{};
  std::vector<int> ids;
  std::vector<int> while_set;
  std::vector<int> while_clear;
};

// 0x222f70: when a record's leader is ready once its flags are all set
// (status 2): given the rumble manager's session check, on regulation 30's
// counter, or never.
enum class SlaiLeaderRule { flags, counter, never };

struct SlaiOpponentTable {
  std::vector<SlaiRoster> rosters;
  // Records whose roster is another than their own id (0x7d).
  std::vector<std::pair<int, int>> record_rosters;
  std::vector<SlaiSubstitution> substitutions;
  std::vector<SlaiLeaderRule> leader_rules; // by regulation
  std::vector<SlaiOpponent> opponents;      // by id, ascending

  [[nodiscard]] bool empty() const noexcept {
    return opponents.empty();
  }
  [[nodiscard]] const SlaiOpponent *opponent(int id) const noexcept;
  // The roster a save record's opponents come from.
  [[nodiscard]] const SlaiRoster *roster_of(int record) const noexcept;
  [[nodiscard]] SlaiLeaderRule leader_rule(int regulation) const noexcept;
};

// Throws std::runtime_error naming the member at fault.
[[nodiscard]] SlaiOpponentTable parse_slai_opponents(const json::JsonValue &value);

} // namespace phantomcrash
