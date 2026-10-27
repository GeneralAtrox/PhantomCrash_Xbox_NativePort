// S.L.A.I.'s story in the screen runner: the talk scene a move between the
// port's screens plays (docs/SLAI_STORY_PLAN.md), and its screen.

#include "desktop_game_screens.hpp"
#include "desktop_screens.hpp"
#include "desktop_slai_talk.hpp"

#include <algorithm>

namespace phantomcrash {
namespace {

constexpr std::string_view slai_talk_prefix = "slai_talk:";
// The town map's place, where "town" rows play.
constexpr std::string_view town_place = "townmap";
// Where a scene started on its own (--start) leads.
constexpr std::string_view default_next = "town_map";

// The script's name placeholders take the port's names; braces would read
// as text tokens.
std::string placeholder_name(const std::string &name, const std::string &fallback) {
  std::string out;
  std::ranges::copy_if(name, std::back_inserter(out), [](char c) { return c != '{' && c != '}'; });
  return out.empty() ? fallback : out;
}

// The CHIP fitted in the hangar ridden, or null.
const StockItem *riding_chip(const Garage &garage) {
  if (garage.riding < 0 || garage.riding >= static_cast<int>(garage.hangars.size())) {
    return nullptr;
  }
  const Hangar &hangar = garage.hangars[static_cast<std::size_t>(garage.riding)];
  const auto chip = garage.stock.find(hangar.part(PartSlot::chip));
  return chip != garage.stock.end() ? &chip->second : nullptr;
}

} // namespace

std::optional<SlaiStory> ScreenRunner::load_slai_story(const DataRoots &roots) {
  if (!roots.slai()) {
    return std::nullopt;
  }
  try {
    auto story = SlaiStory::load(roots);
    if (!story) {
      SDL_Log("S.L.A.I. story: no talk data (register-slai-talk)");
    }
    return story;
  } catch (const std::exception &error) {
    SDL_Log("S.L.A.I. story: talk data unreadable: %s", error.what());
    return std::nullopt;
  }
}

std::string ScreenRunner::slai_story_target(const std::string &source, const std::string &name) {
  slai_story_nights(quest_.slai_story, slai_story_->rules(), quest_.total_days);
  SlaiStoryContext context{quest_.slai_story, quest_.slai_rumble};
  const auto [first_song, last_song] = slai_story_->music_credits();
  context.collections.music =
      static_cast<int>(std::ranges::count_if(quest_.music.owned, [&](const auto &entry) {
        return entry.first >= first_song && entry.first <= last_song;
      }));
  context.junk_shop = slai_story_->junk_shop(name);
  const auto to = slai_story_->places_of(name);
  const SlaiTalkEvent *event = nullptr;
  if (std::ranges::find(to, town_place) != to.end()) {
    event = slai_story_->next_event("town", town_place, town_place, context);
  }
  for (const auto &from : slai_story_->places_of(source)) {
    for (const auto &place : to) {
      if (event == nullptr) {
        event = slai_story_->next_event("scene_change", from, place, context);
      }
    }
  }
  if (event == nullptr) {
    slai_story_->reset(quest_.slai_story);
    return {};
  }
  SlaiStory::complete(*event, quest_.slai_story);
  SDL_Log("S.L.A.I. story event: %s from=%s to=%s", event->name.c_str(), source.c_str(),
          name.c_str());
  return std::string(slai_talk_prefix) + event->name;
}

std::unique_ptr<Screen> ScreenRunner::make_slai_talk(const std::string &name,
                                                     const std::string &from) {
  if (!slai_story_) {
    return nullptr;
  }
  const std::string scene_name = name.substr(slai_talk_prefix.size());
  SlaiTalkSceneNames names;
  names.player = placeholder_name(quest_.player_name, names.player);
  names.day = quest_.day + 1;
  // The ridden CHIP is the partner (VM+0x770, its CPU type in S.L.A.I.).
  SlaiTalkPartner partner;
  const auto &chips = game_->part_tables().chips;
  if (const StockItem *chip = riding_chip(quest_.garage);
      chip != nullptr && chip->id >= 0 && chip->id < static_cast<int>(chips.size())) {
    const ChipRecord &record = chips[static_cast<std::size_t>(chip->id)];
    // S.L.A.I.'s own CHIPs carry their CPU type (2..9: script cases 1..8)
    // and picture; Phantom Crash's stand in by their chatter.
    if (record.slai_type >= 2 && record.slai_type <= 9 && record.face) {
      partner = {record.slai_type - 1, record.face->image};
    } else {
      partner = slai_story_->partner(record.chatter, record.name);
    }
    names.partner = placeholder_name(chip->name.empty() ? record.name : chip->name, names.partner);
  }
  auto scene = slai_story_->scene(scene_name, names, partner);
  if (scene == nullptr) {
    SDL_Log("S.L.A.I. story: no script for %s", scene_name.c_str());
    return nullptr;
  }
  std::string next = slai_next_;
  if (next.empty()) {
    next = from.empty() || from.starts_with(slai_talk_prefix) ? std::string(default_next) : from;
  }
  std::unique_ptr<Screen> behind;
  if (slai_story_->mode(scene_name) == SlaiTalkMode::town) {
    behind = make(next, from);
  }
  return std::make_unique<SlaiTalkScreen>(textures_, std::move(scene), std::move(next),
                                          std::move(behind));
}

} // namespace phantomcrash
