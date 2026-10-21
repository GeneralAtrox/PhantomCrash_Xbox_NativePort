// S.L.A.I.'s story: loading the talk data and choosing the scene a move
// plays (0x26A420 and its conditions 0x26A990..0x26AF40).

#include "phantomcrash/slai_story.hpp"

#include <algorithm>
#include <array>

namespace phantomcrash {
namespace {

// 0x26A570's record ranges: ranker records, then the regional ones.
constexpr int ranker_first = 0x30, ranker_last = 0x67;
constexpr int regional_first = 0x68, regional_last = 0x6E;
// The money conditions' thresholds (0x26AB90, 0x26ABE0, 0x26AC30).
constexpr std::int64_t money_999 = 999, money_572 = 0x23C, money_99 = 99;
// Rows name a city 0..8 (save + 0xA20) or none.
constexpr int last_city = 8;
// The place names that match any place (place 0x32 and up).
constexpr std::string_view any_place = "any";
// The talk table's flag for scenes with their own background (+0x0A bit 1).
constexpr std::string_view talk_background = "talk_background";
// Script languages, the configured one first, then these.
constexpr std::array<std::string_view, 4> languages{"uk", "us", "de", "fr"};

using json::JsonValue;

int cleared_in(const SlaiRumbleSave &rumble, int first, int last) {
  return static_cast<int>(std::ranges::count_if(rumble.records, [&](const auto &entry) {
    return entry.first >= first && entry.first <= last && entry.second.cleared;
  }));
}

bool record_cleared(const SlaiRumbleSave &rumble, int id) {
  const auto found = rumble.records.find(id);
  return found != rumble.records.end() && found->second.cleared;
}

bool flag_set(const SlaiStorySave &save, int flag) {
  return flag >= 0 && static_cast<std::size_t>(flag) < save.flags.size() &&
         save.flags[static_cast<std::size_t>(flag)];
}

bool place_matches(std::string_view row, std::string_view place) {
  return row == any_place || row == place;
}

SlaiTalkSceneOptions options_from(const JsonValue &value, const SlaiTalkSceneOptions &defaults) {
  SlaiTalkSceneOptions options = defaults;
  options.text_rate = static_cast<int>(value.number_or("text_rate", defaults.text_rate));
  options.glitched = value.boolean_or("glitched", defaults.glitched);
  return options;
}

} // namespace

int slai_story_progress(const SlaiRumbleSave &rumble) {
  return cleared_in(rumble, ranker_first, ranker_last) +
         cleared_in(rumble, regional_first, regional_last) + rumble.finals_cleared;
}

bool slai_condition_met(const SlaiTalkEvent &event, int tutorial_record,
                        const SlaiStoryContext &context) {
  const std::string_view condition = event.condition;
  const bool tutorial = record_cleared(context.rumble, tutorial_record);
  if (condition.empty()) {
    return true;
  }
  if (condition == "tutorial_cleared_without_4448") {
    return tutorial && !context.story.tutorial_mark;
  }
  if (condition == "tutorial_cleared_with_4448") {
    return tutorial && context.story.tutorial_mark;
  }
  if (condition == "tutorial_not_cleared") {
    return !tutorial;
  }
  if (condition == "money_over_999") {
    return context.money > money_999;
  }
  if (condition == "money_over_572") {
    return context.money > money_572;
  }
  if (condition == "money_over_99") {
    return context.money > money_99;
  }
  return false; // "never", the sponsors, the completion score, fn_*
}

std::optional<SlaiStory> SlaiStory::load(const DataRoots &roots) {
  const auto table_path = roots.find_asset("tables/slai_talk.json");
  const auto font_path = roots.find_asset("font/glyphs.json");
  if (!table_path || !font_path) {
    return std::nullopt;
  }
  SlaiStory story;
  story.shared_ = std::make_shared<Shared>();
  auto &shared = *story.shared_;
  shared.roots = roots;
  shared.table = slai_talk_table_from_json(json::read_file(*table_path));
  shared.font = slai_talk_font_from_json(json::read_file(*font_path));
  shared.language = std::string(languages[0]);
  if (roots.has_document("story/slai_talk.json")) {
    const auto config = roots.document("story/slai_talk.json");
    shared.language = config.string_or("language", shared.language);
    shared.defaults = options_from(config, shared.defaults);
    if (const auto *scenes = config.find("scenes"); scenes != nullptr && scenes->is_object()) {
      for (const auto &[name, value] : scenes->object()) {
        shared.options[name] = options_from(value, shared.defaults);
      }
    }
    if (const auto *places = config.find("places"); places != nullptr && places->is_object()) {
      for (const auto &[place, screens] : places->object()) {
        for (const auto &screen : screens.array()) {
          shared.places[place].push_back(screen.string());
        }
      }
    }
  }
  return story;
}

SlaiTalkMode SlaiStory::mode(std::string_view name) const {
  const auto *event = shared_->table.event(name);
  return event == nullptr || event->has_flag(talk_background) ? SlaiTalkMode::talk
                                                              : SlaiTalkMode::town;
}

std::unique_ptr<SlaiTalkScene> SlaiStory::scene(std::string_view name,
                                                const SlaiTalkSceneNames &names) const {
  std::vector<std::string_view> order{shared_->language};
  order.insert(order.end(), languages.begin(), languages.end());
  std::optional<std::filesystem::path> path;
  for (const auto language : order) {
    path = shared_->roots->find_asset("talk/event/" + std::string(language) + "/" +
                                      std::string(name) + ".json");
    if (path) {
      break;
    }
  }
  if (!path) {
    return nullptr;
  }
  auto script = slai_talk_script_from_json(json::read_file(*path), shared_->table);
  const auto found = shared_->options.find(name);
  SlaiTalkSceneOptions options =
      found != shared_->options.end() ? found->second : shared_->defaults;
  options.names = names;
  // The loader keeps the shared data alive for as long as the scene is.
  SlaiTalkBankLoader banks = [shared = shared_](const std::string &relative) -> const AnbBank * {
    auto cached = shared->banks.find(relative);
    if (cached == shared->banks.end()) {
      std::unique_ptr<AnbBank> bank;
      if (const auto file = shared->roots->find_asset(relative)) {
        bank = std::make_unique<AnbBank>(anb_from_json(json::read_file(*file)));
      }
      cached = shared->banks.emplace(relative, std::move(bank)).first;
    }
    return cached->second.get();
  };
  return std::make_unique<SlaiTalkScene>(shared_->table, shared_->font, std::move(script),
                                         mode(name), std::move(banks), std::move(options));
}

const SlaiTalkEvent *SlaiStory::next_event(std::string_view kind, std::string_view place,
                                           std::string_view sub_place,
                                           const SlaiStoryContext &context) const {
  const int progress = slai_story_progress(context.rumble);
  for (const auto &event : shared_->table.events) {
    const bool city = event.city < 0 || event.city > last_city || event.city == context.rumble.city;
    const bool flags = (event.required_flag < 0 || flag_set(context.story, event.required_flag)) &&
                       !flag_set(context.story, event.blocking_flag) &&
                       !flag_set(context.story, event.completion_flag);
    if (event.kind == kind && place_matches(event.place, place) &&
        place_matches(event.sub_place, sub_place) && city && progress >= event.threshold && flags &&
        slai_condition_met(event, shared_->table.tutorial_record, context)) {
      return &event;
    }
  }
  return nullptr;
}

void SlaiStory::complete(const SlaiTalkEvent &event, SlaiStorySave &save) {
  if (event.completion_flag >= 0 &&
      static_cast<std::size_t>(event.completion_flag) < save.flags.size()) {
    save.flags[static_cast<std::size_t>(event.completion_flag)] = true;
  }
}

std::vector<std::string> SlaiStory::places_of(std::string_view screen) const {
  std::vector<std::string> out;
  for (const auto &[place, screens] : shared_->places) {
    if (std::ranges::find(screens, screen) != screens.end()) {
      out.push_back(place);
    }
  }
  return out;
}

} // namespace phantomcrash
