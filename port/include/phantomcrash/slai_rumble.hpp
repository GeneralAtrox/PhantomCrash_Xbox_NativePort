#pragma once

// S.L.A.I. Rumble's tables: S.L.A.I.'s Club Wired campaign, converted from
// the user's disc into tables/slai_rumble.json (slai/rumble_table.py), and
// the port's own choices for it in data/world/slai_rumble.json. See
// docs/SLAI_RUMBLE_PLAN.md and docs/SLAI_CAMPAIGN_RESEARCH.md. Addresses are
// PAL SLES-52940; names are researcher-assigned.

#include "phantomcrash/json.hpp"
#include "phantomcrash/slai_opponent_table.hpp"
#include "phantomcrash/slai_part_facts.hpp"
#include "phantomcrash/slai_rota.hpp"

#include <array>
#include <cstdint>
#include <optional>
#include <string>
#include <utility>
#include <vector>

namespace phantomcrash {

class DataRoots;

inline constexpr int slai_no_match = -1;
inline constexpr int slai_any_maker = -1;
// A rule with no target (the Dummy rows' 0xffff read as a signed short).
inline constexpr int slai_no_target = -1;
inline constexpr const char *slai_rumble_table_file = "tables/slai_rumble.json";
inline constexpr const char *slai_rumble_world_file = "world/slai_rumble.json";

// A regulation's special rule (+0x1c, -1 none), as the table names it.
enum class SlaiSpecialRule {
  none,
  rental_only,
  no_optical_camouflage,
  shoulder_weapons_only,
  arm_weapons_only,
  close_range_only,
  lock_on_only,
  grenade_only,
  e_chip_only,
};

// A regulation (0x4227e0, 36 bytes), named by UK_RUMBLETYPENAME.MSG.
struct SlaiRegulation {
  int id{};
  std::string name;
  std::int64_t entry_requirement{};     // +4: the money the player must hold
  int maker{slai_any_maker};            // +8: the body's maker, 0 to 4
  std::vector<std::string> leg_classes; // +0xc..+0x18, by name
  SlaiSpecialRule special{SlaiSpecialRule::none};
  // +0x20: the score the player must reach for the prize (0x24b0b0);
  // 1 head-to-head, 20 to 30 rumbling, slai_no_target on the Dummy rows.
  int target{slai_no_target};
  std::int64_t prize{}; // +0x22
};

// A match template (0x41ece0, 56 bytes).
struct SlaiMatch {
  int id{};
  int regulation{};
  bool night{};
  int robots_min{};
  int robots_max{};
  // +8: the roster the filler drones come from (the table calls it
  // "level"; docs/SLAI_OPPONENTS_RESEARCH.md), -1 for none.
  int pool{-1};
  int save_record{-1};   // +0xa: the record it clears and fills
  int save_record_b{-1}; // +0xc: the cameo roster's record
};

// The finals the code injects at the finals stage (0x257070), as data.
struct SlaiFinalReplace {
  std::vector<int> from;
  // The (day, night) ids that replace them; none means the city's
  // fallback pair.
  std::optional<std::array<int, 2>> with;
};

struct SlaiFinalRound {
  std::string name;
  int host{};
  std::array<int, 2> matches{slai_no_match, slai_no_match};
  bool host_ignores_rota{};
  std::vector<SlaiFinalReplace> replace;
};

// tables/slai_rumble.json.
struct SlaiRumbleTable {
  std::vector<std::string> cities; // by column: 0 Tokyo ... 6 Las Vegas
  std::vector<int> display_order;  // the schedule screen's (0x43acf0)
  int schedule_days{};
  int stages{};
  std::vector<SlaiRegulation> regulations;
  std::vector<SlaiMatch> matches;
  // Each rota day's city-day row, by city (0x42d0f0 +10).
  std::vector<std::vector<int>> day_rows;
  // Each city-day row's (day, night) match ids by stage (0x422ce0).
  std::vector<std::vector<std::array<int, 2>>> city_days;
  int finals_stage{};
  std::vector<SlaiFinalRound> finals;              // by finals cleared
  std::vector<std::array<int, 2>> finals_fallback; // by city (0x334a80)
  // The campaign's opponents; empty in a table converted before they were.
  SlaiOpponentTable opponents;
  // What the entry check reads of the imported S.L.A.I. parts; empty as
  // the opponents.
  SlaiPartFacts parts;
};

// A city's arena: the imported stages (data/world/stages.json) its day and
// night matches are fought on. The disc has no night scene for Las Vegas,
// whose night matches the original fights on the day scene.
struct SlaiRumbleCity {
  std::string name;
  std::string day_stage;
  std::string night_stage;
};

// The records the stage ladder counts (0x257410 to 0x2574e0), as ranges of
// save record ids, first and one past the last.
struct SlaiRumbleLadder {
  int tutorial_record{0x18};
  std::array<int, 2> ranker_records{0x30, 0x68};
  int rankers_needed{0x38};
  std::array<int, 2> regional_records{0x68, 0x6f};
  int regionals_needed{7};
  std::array<int, 2> finals_records{0x1a, 0x1d};
  int finals_needed{3};
};

// When a record's leader joins (0x2225a0, 0x223ed0; by regulation):
// open from the start, after its first `flags` members are all beaten,
// or held back (status 2, regulation 30).
struct SlaiStatusRule {
  enum class Kind { open, flags, held };
  Kind kind{Kind::open};
  int flags{};
};

// What the special rules test on the robot's parts (0x259a40).
struct SlaiSpecialValues {
  std::vector<int> close_range_classes{2}; // weapon +0x134
  std::vector<int> lock_on_kinds{1, 9};    // weapon +0x18
  std::vector<int> grenade_classes{3, 15}; // weapon +0x134
  std::vector<int> e_chips{0, 1};          // the CHIP record
};

// Balance knobs; the defaults are the original's.
struct SlaiRumbleBalance {
  double entry_scale{1.0};
  bool charge_entry{}; // the original only compares the money (0x259a40)
  double prize_scale{1.0};
  int robots_max{6}; // the rumble manager's cap (0x222070)
};

// How a rumble's battle is laid out (port choices; the "battle" block).
struct SlaiBattleRules {
  // Drones roam the stage's own player spawns, planned over the navigation
  // grid, rather than the training ground's route (TrainingBattle).
  bool spawn_route{true};
  float spawn_merge{3.0F}; // units: spawns closer than this are one
  // Stand-ins stuck this long hop, then move on (BattleSettings
  // drone_unstick_seconds); 0 never.
  float unstick_seconds{0.0F};
  float hop_seconds{0.3F};
  int hops{2};
};

// An opponent's AI chip by its level, the last row whose level it reaches
// (a port choice: S.L.A.I.'s CPU rows are undecoded).
struct SlaiOpponentChip {
  int from_level{};
  int chip{-1}; // tables/parts.json chips
  int chip_level{};
};

// How the campaign's opponents take the field (the "opponents" block): what
// the original's rules (slai_lineup.hpp) leave open.
struct SlaiOpponentRules {
  // 0x222a60 lets these body makers into any rumble (the drones').
  std::vector<int> always_eligible_makers{8, 9};
  // Story flags treated as set besides the save's (save + 0x426f), which
  // 0x256cc0's substitutions test; none by default.
  std::vector<int> story_flags;
  // 0x222f70's unresolved inputs: the rumble manager's session check, and
  // regulation 30's counter.
  bool leader_session{true};
  bool leader_counter{};
  std::vector<SlaiOpponentChip> chips;
  // The AI profile (contests.hpp Entrant) of each behaviour value; others
  // take `profile`.
  std::vector<std::pair<int, int>> profiles;
  int profile{4};
  // Every opponent's paint and sticker, -1 keeping its record's (the
  // original's 16-region paint is undecoded).
  int paint{-1};
  int emblem{-1};
  // A place emptied by a shootdown draws its next opponent this long after.
  float arrival_seconds{8.0F};
  // Shooting a pool drone down sets its pool roster's flag as well.
  bool pool_flags{};
};

// data/world/slai_rumble.json.
struct SlaiRumbleWorld {
  std::vector<SlaiRumbleCity> cities; // by column
  std::vector<std::string> makers;    // by maker id: Carro, Proton, ...
  // Each leg class's S.L.A.I. leg types (0x24d7d0); a class not listed
  // allows any legs.
  std::vector<std::pair<std::string, std::vector<int>>> leg_classes;
  SlaiSpecialValues special;
  std::vector<SlaiStatusRule> status_rules; // by regulation
  SlaiRumbleLadder ladder;
  SlaiRumbleBalance balance;
  SlaiBattleRules battle;
  SlaiOpponentRules opponents;
  SlaiEntryRules entry;
  int start_city{};
};

// Throw std::runtime_error naming the member at fault.
[[nodiscard]] SlaiRumbleTable parse_slai_rumble_table(const json::JsonValue &document);
[[nodiscard]] SlaiRumbleWorld parse_slai_rumble_world(const json::JsonValue &document);

struct SlaiRumbleData {
  SlaiRumbleTable table;
  SlaiRumbleWorld world;
  SlaiRota rota; // data/world/slai_rota.json
};

// The converted table through the asset roots (so mods/tables/ replaces
// it), and the world and rota documents through the data roots. Nothing
// when the table is not installed.
[[nodiscard]] std::optional<SlaiRumbleData> load_slai_rumble(const DataRoots &roots);

[[nodiscard]] const SlaiMatch *slai_match(const SlaiRumbleTable &table, int id);
[[nodiscard]] const SlaiRegulation *slai_regulation(const SlaiRumbleTable &table, int id);
// The stage a match is fought on: the city's day or night stage.
[[nodiscard]] const std::string &slai_match_stage(const SlaiRumbleWorld &world, int city,
                                                  bool night);

} // namespace phantomcrash
