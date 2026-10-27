#pragma once

// What S.L.A.I.'s story keeps count of between scenes (docs/
// SLAI_STORY_PLAN.md, "Counts and sponsors"): the nights, the enemies shot
// down, the sponsors and the completion score. The rules come from the
// talk table the S.L.A.I. registration writes (tables/slai_talk.json).

#include "phantomcrash/json.hpp"
#include "phantomcrash/slai_rumble_save.hpp"
#include "phantomcrash/slai_story_save.hpp"

#include <array>
#include <cstdint>
#include <functional>
#include <map>
#include <string>
#include <vector>

namespace phantomcrash {

// A "defeated_over_*" condition: more than `above` shot down; once the
// `superseded_by` flag is set it sets `marks` instead of playing.
struct SlaiDefeatMilestone {
  std::int64_t above{};
  int superseded_by{-1}, marks{-1};
};

// 0x24A9E0 (losing a sponsor) and 0x25A4E0 (an offer). Sponsors are
// numbered 0 Hartman, 1 Proton, 2 Carro, 3 KHT, 4 Zwerg; brands are the
// body table's +2 (the regulation makers, and 5..7).
struct SlaiSponsorRules {
  std::vector<int> offer_flags;
  int offer_score_above{};
  std::map<int, int> regulation_makers; // regulation -> sponsor
  std::vector<int> brand_regulations;   // the offer follows the body's brand
  std::map<int, int> brand_makers;      // brand -> sponsor
  int keep_score_below{};
  int streak_limit{};
  std::map<int, std::vector<int>> sponsor_brands; // sponsor -> the brands it keeps
  std::vector<int> head_to_head_matches;
};

// 0x24B5A0: each part's count capped and scaled to its points; the sum is
// divided by `divisor` and rounded half up.
struct SlaiCompletionPart {
  std::string source; // records, collections, decals, music, sponsors
  int first{}, count{}, points{};
};

struct SlaiStoryRules {
  std::map<std::string, SlaiDefeatMilestone, std::less<>> milestones;
  SlaiSponsorRules sponsors;
  int completion_divisor{1};
  std::vector<SlaiCompletionPart> completion;
  std::vector<std::array<int, 2>> overnight_flags; // (when set, set)
};

[[nodiscard]] SlaiStoryRules slai_story_rules_from_json(const json::JsonValue &table);

// What the save + 0x4220 collections hold in the port: counts by the
// completion parts' sources (collections, decals, music).
struct SlaiCollections {
  int collections{}, decals{}, music{};
};

// A S.L.A.I. match's result as the story reads it (global + 0x489C).
struct SlaiBattleOutcome {
  int match{-1};
  int regulation{-1};
  int score{};
  int shootdowns{};
  bool survived{};
  // + 0x489E: a battle that leaves the sponsors alone (not yet traced; the
  // port sets it for none).
  bool excluded{};
  int brand{-1}; // the ridden body's, -1 for none (a Phantom Crash body)
};

// The days catch up with the port's nights: one each, up to 9999, and each
// night's flag pairs (0x4E8D70). The first look only notes the count.
void slai_story_nights(SlaiStorySave &save, const SlaiStoryRules &rules, int total_days);

// After a S.L.A.I. match: the enemies shot down, the result, the sponsor
// lost (0x24A9E0) and the one offered (0x25A4E0).
void slai_story_after_battle(SlaiStorySave &save, const SlaiStoryRules &rules,
                             const SlaiBattleOutcome &outcome);

// 0x24B5A0: 0..1000; more than 999 is everything.
[[nodiscard]] int slai_completion_score(const SlaiStoryRules &rules, const SlaiStorySave &save,
                                        const SlaiRumbleSave &rumble,
                                        const SlaiCollections &collections);

[[nodiscard]] bool slai_story_flag(const SlaiStorySave &save, int flag);
void slai_set_story_flag(SlaiStorySave &save, int flag);

} // namespace phantomcrash
