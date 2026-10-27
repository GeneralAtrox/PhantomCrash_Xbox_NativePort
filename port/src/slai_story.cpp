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
// 0x26AC80: a completion score above this is everything.
constexpr int completion_full = 999;
// The talk table's flag for rows 0x26A7D0 makes playable again.
constexpr std::string_view reset_flag = "reset";
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
  return slai_story_flag(save, flag);
}

// The count a collection condition needs: its completion part's.
int collection_size(const SlaiStoryRules &rules, std::string_view source) {
  const auto found = std::ranges::find(rules.completion, source, &SlaiCompletionPart::source);
  return found != rules.completion.end() ? found->count : 0;
}

bool collected(const SlaiStoryRules &rules, std::string_view source, int count) {
  const int size = collection_size(rules, source);
  return size > 0 && count >= size;
}

// 0x26AD40..0x26AF40: a sponsor has just come (current differs from the
// previous one) or just gone.
bool sponsor_condition(std::string_view condition, int maker, const SlaiStorySave &story,
                       const SlaiSponsorRules &rules) {
  if (story.sponsor == story.previous_sponsor) {
    return false;
  }
  const bool due = std::ranges::any_of(rules.offer_flags,
                                       [&story](int flag) { return slai_story_flag(story, flag); });
  if (condition == "sponsor_offer_open") {
    return story.sponsor == maker && !due;
  }
  if (condition == "sponsor_offer_due") {
    return story.sponsor == maker && due;
  }
  if (condition == "sponsor_current") {
    return story.sponsor == maker;
  }
  constexpr std::string_view level = "sponsor_level_";
  if (condition.starts_with(level) && condition.size() == level.size() + 1) {
    const int reason = condition.back() - '0';
    return story.previous_sponsor == maker && story.dismissal == reason;
  }
  return false;
}

// 0x26AB60..0x26AC30: more than so many shot down, unless the next
// milestone has played, when this one is marked done instead.
bool milestone(const SlaiDefeatMilestone &rule, SlaiStorySave &story) {
  if (slai_story_flag(story, rule.superseded_by)) {
    slai_set_story_flag(story, rule.marks);
    return false;
  }
  return story.defeated > rule.above;
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

bool slai_condition_met(const SlaiTalkEvent &event, const SlaiTalkTable &table,
                        const SlaiStoryRules &rules, const SlaiStoryContext &context) {
  const std::string_view condition = event.condition;
  const auto &story = context.story;
  const bool tutorial = record_cleared(context.rumble, table.tutorial_record);
  if (condition.empty()) {
    return true;
  }
  if (condition == "tutorial_cleared_first_day") {
    return tutorial && story.days == 0;
  }
  if (condition == "tutorial_cleared_later") {
    return tutorial && story.days != 0;
  }
  if (condition == "tutorial_not_cleared") {
    return !tutorial;
  }
  if (condition == "last_battle_lost") {
    return !story.survived;
  }
  if (condition == "last_battle_survived") {
    return story.survived;
  }
  if (condition == "all_mods_collected") {
    return collected(rules, "collections", context.collections.collections);
  }
  if (condition == "all_decals_collected") {
    return collected(rules, "decals", context.collections.decals);
  }
  if (condition == "all_music_collected") {
    return collected(rules, "music", context.collections.music);
  }
  if (condition == "completion_score") {
    return slai_completion_score(rules, story, context.rumble, context.collections) >
           completion_full;
  }
  if (condition == "junk_module_shop") {
    return context.junk_shop;
  }
  if (const auto found = rules.milestones.find(condition); found != rules.milestones.end()) {
    return milestone(found->second, context.story);
  }
  if (condition.starts_with("sponsor_")) {
    return sponsor_condition(condition, event.condition_arg, story, rules.sponsors);
  }
  return false; // "never" and the unknown
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
  const auto table_json = json::read_file(*table_path);
  shared.table = slai_talk_table_from_json(table_json);
  shared.rules = slai_story_rules_from_json(table_json);
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
    if (const auto *shops = config.find("junk_shops"); shops != nullptr && shops->is_array()) {
      for (const auto &shop : shops->array()) {
        shared.junk_shops.push_back(shop.string());
      }
    }
    if (const auto *credits = config.find("music_credits");
        credits != nullptr && credits->is_array() && credits->array().size() == 2) {
      shared.music_credits = {static_cast<int>(credits->array()[0].number()),
                              static_cast<int>(credits->array()[1].number())};
    }
    if (const auto *partner = config.find("partner"); partner != nullptr && partner->is_object()) {
      if (const auto *selectors = partner->find("selectors");
          selectors != nullptr && selectors->is_array()) {
        for (const auto &selector : selectors->array()) {
          shared.partner_selectors.push_back(static_cast<int>(selector.number()));
        }
      }
      if (const auto *pictures = partner->find("pictures");
          pictures != nullptr && pictures->is_object()) {
        for (const auto &[family, picture] : pictures->object()) {
          shared.partner_pictures[family] = picture.string();
        }
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
                                                const SlaiTalkSceneNames &names,
                                                const SlaiTalkPartner &partner) const {
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
  options.partner = partner;
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
        slai_condition_met(event, shared_->table, shared_->rules, context)) {
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

void SlaiStory::reset(SlaiStorySave &save) const {
  for (const auto &event : shared_->table.events) {
    if (event.has_flag(reset_flag) && event.completion_flag >= 0 &&
        static_cast<std::size_t>(event.completion_flag) < save.flags.size()) {
      save.flags[static_cast<std::size_t>(event.completion_flag)] = false;
    }
  }
}

SlaiTalkPartner SlaiStory::partner(int chatter, std::string_view family) const {
  const auto &selectors = shared_->partner_selectors;
  const auto picture = shared_->partner_pictures.find(family);
  if (chatter < 0 || chatter >= static_cast<int>(selectors.size()) ||
      picture == shared_->partner_pictures.end()) {
    return {};
  }
  return {selectors[static_cast<std::size_t>(chatter)], picture->second};
}

bool SlaiStory::junk_shop(std::string_view screen) const {
  return std::ranges::find(shared_->junk_shops, screen) != shared_->junk_shops.end();
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
