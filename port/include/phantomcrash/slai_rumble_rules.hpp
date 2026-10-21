#pragma once

// S.L.A.I. Rumble's rules (docs/SLAI_RUMBLE_PLAN.md, phase 2): what the
// rota offers, who may enter, the records a battle fills, the prize and
// the stage ladder. Pure functions over the tables (slai_rumble.hpp) and
// the save (slai_rumble_save.hpp); campaign.py mirrors the same code and
// gives the tests their expected values. PAL SLES-52940 addresses.

#include "phantomcrash/settings.hpp"
#include "phantomcrash/slai_rumble.hpp"
#include "phantomcrash/slai_rumble_save.hpp"

#include <array>
#include <cstdint>
#include <optional>
#include <utility>
#include <vector>

namespace phantomcrash {

// 0x257070: the match a city holds in a slot on a rota day at a stage, or
// slai_no_match. The day wraps at the table's schedule_days (0x257350); at
// the finals stage the finals cleared so far inject the next final.
[[nodiscard]] int slai_scheduled_match(const SlaiRumbleTable &table, int day, int city, int stage,
                                       bool night, int finals_cleared);
// A city's rumble on a calendar day, at the save's stage: the S.L.A.I.
// day the rota borrows its line-up from, and the match that day shows
// (its day slot's, else its night slot's).
struct SlaiCityMatch {
  int city{};
  int slai_day{};
  int match{slai_no_match};
  bool night{};
};
// The city's rumble on calendar day `day`, or nothing while it is closed.
[[nodiscard]] std::optional<SlaiCityMatch>
slai_city_match(const SlaiRumbleData &data, int day, int city, const SlaiRumbleSave &save);
// The day's open cities' rumbles, in the rota's (display) order.
[[nodiscard]] std::vector<SlaiCityMatch>
slai_day_matches(const SlaiRumbleData &data, int day, const SlaiRumbleSave &save);
// Today's (day, night) matches in the save's city; none while it is closed.
[[nodiscard]] std::array<int, 2> slai_todays_matches(const SlaiRumbleData &data, int day,
                                                     const SlaiRumbleSave &save);

// The records cleared among save record ids [first, last) (0x257440,
// 0x257490, 0x2574e0).
[[nodiscard]] int slai_records_cleared(const SlaiRumbleSave &save,
                                       const std::array<int, 2> &records);
[[nodiscard]] bool slai_record_cleared(const SlaiRumbleSave &save, int record);
// 0x25a4e0: the stage after a battle; it moves on at most one step.
[[nodiscard]] int slai_next_stage(const SlaiRumbleSave &save, const SlaiRumbleLadder &ladder);

// The parts of the ridden robot the entry check reads (0x259a40), in
// S.L.A.I.'s own terms.
struct SlaiWeapon {
  int weapon_class{}; // +0x134: 2 close range, 3 or 15 grenade
  int aim_kind{};     // +0x18: 1 or 9 lock-on
};
struct SlaiRobot {
  bool ready{};                                  // 0x259fc0: a body, legs and a CHIP fitted
  int maker{slai_any_maker};                     // the body's maker (+2)
  int leg_type{-1};                              // 0x24d7d0, 0..10
  bool optical_camouflage{};                     // its modules' camouflage adds above 0 (0x257dd0)
  std::array<std::optional<SlaiWeapon>, 2> arms; // hangar + 4, + 6
  std::array<std::optional<SlaiWeapon>, 2> shoulders; // hangar + 8, + 0xa
  int chip{-1};
};

// Why an entry is refused, in the order 0x259a40 checks.
enum class SlaiEntry { allowed, money, not_ready, maker, legs, special };

[[nodiscard]] std::int64_t slai_entry_requirement(const SlaiRumbleWorld &world,
                                                  const SlaiRegulation &regulation);
// 0x259a40, for the player's own robot: the money held, a ready robot, the
// body's maker, one of the leg classes, and the special rule. Rental-only
// rumbles refuse it (the original's case 0).
[[nodiscard]] SlaiEntry slai_entry_check(const SlaiRumbleWorld &world,
                                         const SlaiRegulation &regulation, std::int64_t money,
                                         const SlaiRobot &robot);
// Enters a match: takes the entry requirement when charge_entry is set.
// False when the money falls short.
bool slai_pay_entry(QuestSave &quest, const SlaiRumbleWorld &world,
                    const SlaiRegulation &regulation);

// A match's robot count, the player's among them, under robots_max.
[[nodiscard]] std::pair<int, int> slai_match_robots(const SlaiRumbleWorld &world,
                                                    const SlaiMatch &match);

// A record's status as the rumble manager keeps it (0x2225a0, 0x223ed0):
// the leader may join once it is leader_open; members_beaten waits for the
// manager's session check (opponent_rules.py, leader_ready).
enum class SlaiRecordStatus { open_members = 0, members_beaten = 2, leader_open = 3 };
[[nodiscard]] SlaiRecordStatus slai_record_status(const SlaiRumbleWorld &world, int regulation,
                                                  const SlaiRumbleRecord &record);
// An opponent beaten: flag 0 (the leader) clears the record, 1..14 set its
// member's flag. False when nothing changed or the flag is out of range.
bool slai_record_beat(SlaiRumbleSave &save, int record, int flag);

// 0x24b0b0: the prize once the player's score reaches the regulation's
// target, scaled by prize_scale; 0 for a rule with no target.
[[nodiscard]] std::int64_t slai_prize(const SlaiRumbleWorld &world,
                                      const SlaiRegulation &regulation, int score);

// What a battle reports back.
struct SlaiMatchResult {
  int match{slai_no_match};
  int score{};
  std::vector<std::pair<int, int>> beaten; // (save record, flag)
};
struct SlaiMatchOutcome {
  std::int64_t prize{};
  bool cleared{}; // the match's own record became cleared
  int stage_before{};
  int stage_after{};
};
// The records filled, the finals counted, the prize paid into the shared
// wallet (up to money_limit) and the stage moved on.
SlaiMatchOutcome slai_finish_match(QuestSave &quest, const SlaiRumbleData &data,
                                   const SlaiMatchResult &result);

} // namespace phantomcrash
