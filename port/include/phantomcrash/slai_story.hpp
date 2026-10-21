#pragma once

// S.L.A.I.'s story: the talk table, font and scripts from the S.L.A.I.
// registration, the port's choices for them (data/story/slai_talk.json),
// and the story event selector 0x26A420 that picks the scene a move plays
// (docs/SLAI_STORY_PLAN.md).

#include "phantomcrash/data_roots.hpp"
#include "phantomcrash/slai_rumble_save.hpp"
#include "phantomcrash/slai_story_save.hpp"
#include "phantomcrash/slai_talk_scene.hpp"

#include <cstdint>
#include <map>
#include <memory>
#include <optional>
#include <string>
#include <string_view>
#include <vector>

namespace phantomcrash {

// What the event rows test (0x26A420 and the conditions 0x26A990..).
struct SlaiStoryContext {
  const SlaiStorySave &story;
  const SlaiRumbleSave &rumble;
  std::int64_t money{}; // save + 0x4438, the shared NY
};

// 0x26A570's progress: won ranker records, then regional, then finals, as
// one count the rows' thresholds compare with.
[[nodiscard]] int slai_story_progress(const SlaiRumbleSave &rumble);

// Whether a row's condition function holds. Those the port has nothing for
// yet (the sponsors, the completion score, the unnamed ones) do not.
[[nodiscard]] bool slai_condition_met(const SlaiTalkEvent &event, int tutorial_record,
                                      const SlaiStoryContext &context);

class SlaiStory {
public:
  // Null when the S.L.A.I. talk data is not installed.
  [[nodiscard]] static std::optional<SlaiStory> load(const DataRoots &roots);

  [[nodiscard]] const SlaiTalkTable &table() const {
    return shared_->table;
  }
  [[nodiscard]] const std::string &language() const {
    return shared_->language;
  }

  // A scene ready to play; null when its script is missing.
  [[nodiscard]] std::unique_ptr<SlaiTalkScene> scene(std::string_view name,
                                                     const SlaiTalkSceneNames &names) const;
  // Talk scenes have their own background; the others play over the town.
  [[nodiscard]] SlaiTalkMode mode(std::string_view name) const;

  // The first row in story order that plays for this move: kind "town" on
  // the town map (place and sub-place "townmap"), "scene_change" from
  // `place` to `sub_place`, with the row's city, threshold, condition and
  // flags met. Places are S.L.A.I.'s names (the event table's).
  [[nodiscard]] const SlaiTalkEvent *next_event(std::string_view kind, std::string_view place,
                                                std::string_view sub_place,
                                                const SlaiStoryContext &context) const;
  // The row's completion flag is set: it does not play again.
  static void complete(const SlaiTalkEvent &event, SlaiStorySave &save);

  // S.L.A.I.'s place names for one of the port's screens (data/story/
  // slai_talk.json "places"); empty when the screen has none.
  [[nodiscard]] std::vector<std::string> places_of(std::string_view screen) const;

private:
  struct Shared {
    std::optional<DataRoots> roots;
    SlaiTalkTable table;
    SlaiTalkFont font;
    std::string language;
    SlaiTalkSceneOptions defaults;
    std::map<std::string, SlaiTalkSceneOptions, std::less<>> options;
    std::map<std::string, std::vector<std::string>, std::less<>> places;
    std::map<std::string, std::unique_ptr<AnbBank>, std::less<>> banks;
  };
  std::shared_ptr<Shared> shared_;
};

} // namespace phantomcrash
