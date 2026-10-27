#include "desktop_game_screens.hpp"

#include "desktop_battle_music.hpp"
#include "desktop_chip_shop.hpp"
#include "desktop_dealer.hpp"
#include "desktop_assemble.hpp"
#include "desktop_hangar_menu.hpp"
#include "desktop_module_shop.hpp"
#include "desktop_tune_shop.hpp"
#include "desktop_used_sv.hpp"
#include "desktop_front_end.hpp"
#include "desktop_records.hpp"
#include "desktop_rumbling.hpp"
#include "desktop_schedule.hpp"
#include "desktop_slai_rumble.hpp"
#include "desktop_sleep.hpp"
#include "desktop_sonic_amp.hpp"
#include "desktop_sv_spec.hpp"
#include "phantomcrash/added_chips.hpp"
#include "phantomcrash/slai_entry_robot.hpp"

#include <SDL3/SDL_log.h>

namespace phantomcrash {

std::optional<json::JsonValue> converted_table(const DataRoots &roots, const char *relative) {
  if (auto table = read_converted_table(roots, relative)) {
    return table;
  }
  SDL_Log("Table %s is missing; convert the game files again", relative);
  return std::nullopt;
}

GameScreens::GameScreens(TextureCache &textures, const DataRoots &roots, const GlyphFont &font,
                         const FrameStyle &frame, const std::vector<MusicCredit> &credits,
                         const std::vector<StoryEvent> &events)
    : textures_(textures), font_(font), frame_(frame), credits_(credits), events_(events),
      sleep_(parse_sleep_script(roots.document("ui/sleep.json"))),
      schedule_(parse_schedule(roots.document("ui/schedule.json"))),
      records_(parse_records(roots.document("ui/records.json"))),
      battle_music_(parse_battle_music(roots.document("ui/battle_music.json"))),
      sonic_amp_(parse_sonic_amp(roots.document("ui/sonic_amp.json"))),
      dealer_(parse_dealer(roots.document("ui/dealer.json"))),
      used_sv_(parse_used_sv(roots.document("ui/used_sv.json"))),
      buy_chip_(parse_chip_shop(roots.document("ui/chip_shop.json"), ChipShopMode::buy)),
      sell_chip_(parse_chip_shop(roots.document("ui/chip_shop.json"), ChipShopMode::sell)),
      uplift_(parse_chip_shop(roots.document("ui/uplift.json"), ChipShopMode::uplift)),
      buy_module_(parse_module_shop(roots.document("ui/module_shop.json"), ModuleShopMode::buy)),
      sell_module_(parse_module_shop(roots.document("ui/module_shop.json"), ModuleShopMode::sell)),
      stock_module_(parse_stock_module(roots.document("ui/stock_module.json"), sell_module_)),
      customize_(parse_tune_shop(roots.document("ui/customize.json"))),
      hangar_(parse_hangar_menu(roots.document("ui/hangar.json"))),
      assemble_(parse_assemble(roots.document("ui/assemble.json"))),
      sv_spec_(parse_sv_spec(roots.document("ui/sv_spec.json"))),
      robot_preview_(parse_robot_preview(roots.document("ui/robot_preview.json"))),
      rumbling_(parse_rumbling(roots.document("ui/rumbling.json"))),
      rumbling_rules_(parse_rumbling_rules(roots.document("world/rumbling.json"))),
      part_stats_(load_part_stats(roots)),
      message_box_(parse_message_box(roots.document("ui/message_box.json"))),
      keep_sv_(parse_keep_sv(roots.document("ui/keep_sv.json"))),
      vs_mode_(parse_vs_mode(roots.document("ui/vs_mode.json"))) {
  const BattleHudStyle hud = parse_battle_hud_style(roots.document("ui/battle_hud.json"));
  use_reticle_sizes(buy_chip_, hud);
  use_reticle_sizes(sell_chip_, hud);
  use_reticle_sizes(uplift_, hud);
  load_shop_variants(roots, hud);
  assemble_.paint_swatches = buy_module_.paint_swatches;
  assemble_.emblem_swatches = buy_module_.emblem_swatches;
  schedule_.skip.sleep = sleep_;
  if (const auto town = converted_table(roots, "tables/town.json")) {
    contests_ = parse_contest_tables(*town);
  }
  if (const auto parts = converted_table(roots, "tables/parts.json")) {
    parts_ = parse_part_tables(*parts);
  }
  if (const auto garage = converted_table(roots, "tables/garage.json")) {
    garage_ = parse_garage_tables(*garage);
  }
  garage_.tuned_share = part_stats_.tuned_share;
  slai_rumble_ = load_slai_rumble(roots);
  used_sv_shops_.emplace("wild_machines_used_sv", UsedSvShop{used_sv_, ShopStock::phantom_crash});
  load_slai_shops(roots);
  if (slai_rumble_) {
    for (const std::string screen : {"slai_rumble", "slai_battle_school"}) {
      if (const std::string document = "ui/" + screen + ".json"; roots.has_document(document)) {
        slai_hubs_.emplace(screen, parse_slai_rumble_hub(roots.document(document)));
      }
    }
  }
  const auto placeholders = roots.document("ui/placeholders.json");
  if (const auto *buttons = placeholders.find("buttons")) {
    placeholder_buttons_ = buttons_from_json(*buttons);
  }
  placeholder_return_ = placeholders.string_or("return_to", "main_menu");
  if (const auto *screens = placeholders.find("screens")) {
    for (const auto &[name, value] : screens->object()) {
      placeholders_[name] = {value.string_or("label", name), value.string_or("music", ""),
                             value.string_or("return_to", placeholder_return_)};
    }
  }
}

// The hangar comes back to the hangar chosen with its menu open on the
// item that left it (0x01eb3a).
std::unique_ptr<Screen> GameScreens::make_hangar(const std::string &from,
                                                 const QuestSave &quest) const {
  int &remembered = dealer_cursors_["hangar"];
  std::optional<HangarAction> resume;
  for (const auto &[action, screen] : hangar_.routes) {
    if (screen == from) {
      resume = action;
    }
  }
  // The screens the hangar's menu opens come back to it and keep its way out.
  if (hangar_.origins.contains(from)) {
    hangar_exit_ = from;
    resume.reset();
  } else if (from == hangar_.exit_to || hangar_exit_.empty()) {
    hangar_exit_ = hangar_.exit_to;
  }
  HangarMenuScript script = hangar_from(hangar_, hangar_exit_);
  script.exit_to = hangar_exit_;
  HangarMenu logic(std::move(script), quest, parts_, resume ? remembered : 0, resume);
  return std::make_unique<HangarMenuScreen>(textures_, font_, std::move(logic), message_box_, from,
                                            frame_, credits_, quest, remembered, preview_world(),
                                            robot_preview_);
}

// RUMBLING comes back from SELECT SV (the hangar) to the area's sub-menu
// (0x016d0e).
std::unique_ptr<Screen> GameScreens::make_rumbling(const std::string &from,
                                                   const QuestSave &quest) const {
  int &remembered = dealer_cursors_["rumbling"];
  std::optional<int> resume;
  if (from == rumbling_.hangar_route) {
    resume = remembered;
  }
  Rumbling logic(rumbling_, contests_, regulation_check(), quest, resume);
  return std::make_unique<RumblingScreen>(textures_, font_, std::move(logic), contests_,
                                          message_box_, from, frame_, credits_, quest, remembered);
}

// ASSEMBLE works on the hangar the hangar's wheel has chosen, coming back
// to the cell that left it (NAME, SPEC).
std::unique_ptr<Screen> GameScreens::make_assemble(const std::string &from,
                                                   const QuestSave &quest) const {
  std::optional<AssembleItem> resume;
  for (const auto &[item, screen] : assemble_.routes) {
    if (screen == from) {
      resume = item;
    }
  }
  Assemble logic(assemble_, quest, parts_, garage_, part_stats_, dealer_cursors_["hangar"], resume);
  return std::make_unique<AssembleScreen>(textures_, font_, std::move(logic), message_box_, from,
                                          frame_, credits_, quest, preview_world(), robot_preview_);
}

// SV SPEC shows the SV the screen that opened it has chosen: a dealer's or
// the used SVs' offer, KEEP SV's kept Scoobee, or the hangar the hangar's
// wheel has chosen.
std::unique_ptr<Screen> GameScreens::make_sv_spec(const std::string &from,
                                                  const std::string &place_before,
                                                  const QuestSave &quest) const {
  std::optional<std::vector<SvOffer>> offers;
  int chosen = -1;
  if (dealer_.dealers.contains(from)) {
    offers = dealer_lineup(from, quest);
    // Dealer cursors include COMPANY PROFILE before the three SV offers.
    chosen = dealer_cursors_[from] - 1;
  } else if (const auto used = used_sv_shops_.find(from); used != used_sv_shops_.end()) {
    offers = used_sv_lineup(used->second, quest);
    chosen = dealer_cursors_[from];
  }
  SpecRobot robot;
  const int kept = keep_cursor_.first + keep_cursor_.row;
  if (from == "keep_sv" && kept >= 0 && kept < static_cast<int>(vs_scoobees_.size())) {
    const VsScoobee &scoobee = vs_scoobees_[static_cast<std::size_t>(kept)];
    robot = record_spec(scoobee.robot, parts_, garage_);
    const Hangar &bay = robot.garage.hangars[static_cast<std::size_t>(robot.hangar)];
    if (const auto chip = robot.garage.stock.find(bay.part(PartSlot::chip));
        chip != robot.garage.stock.end() && !scoobee.chip_name.empty()) {
      chip->second.name = scoobee.chip_name;
    }
  } else if (!offers) {
    robot = hangar_spec(quest.garage, dealer_cursors_["hangar"]);
  } else if (chosen >= 0 && chosen < static_cast<int>(offers->size())) {
    robot = record_spec((*offers)[static_cast<std::size_t>(chosen)].robot, parts_, garage_);
  }
  if (offers) {
    SDL_Log("SV Spec: shop=%s menu=%d offer=%d name=%s", from.c_str(), dealer_cursors_[from],
            chosen, robot.name.c_str());
  }
  SvSpec logic(sv_spec_, std::move(robot), parts_, garage_, part_stats_,
               resolve(sv_spec_.exit_to, place_before));
  return std::make_unique<SvSpecScreen>(textures_, font_, std::move(logic), from, frame_, credits_,
                                        quest, preview_world(), robot_preview_);
}

// "back" returns to the place a screen was opened from, past any story event.
std::string GameScreens::resolve(const std::string &target, const std::string &place_before) const {
  if (target != "back") {
    return target;
  }
  return place_before.empty() ? placeholder_return_ : place_before;
}

std::unique_ptr<Screen> GameScreens::make(const std::string &name, const std::string &from,
                                          const std::string &place_before,
                                          const QuestSave &quest) const {
  if (name == "sleep") {
    return std::make_unique<SleepScreen>(textures_.renderer(), sleep_, quest);
  }
  if (name == "schedule") {
    return std::make_unique<ScheduleScreen>(textures_, font_, schedule_, contests_, from,
                                            resolve(schedule_.exit_to, place_before), frame_,
                                            credits_, quest, message_box_, slai_rumble());
  }
  if (const auto hub = slai_hubs_.find(name); hub != slai_hubs_.end() && slai_rumble_) {
    const RegulationCheck check = regulation_check();
    return std::make_unique<SlaiRumbleScreen>(
        textures_, font_, hub->second, *slai_rumble_, from,
        resolve(hub->second.exit_to, place_before), frame_, credits_, quest, message_box_,
        slai_riding_robot(*slai_rumble_, check, quest),
        !slai_rental_robots(slai_rumble_->world, check.rules).empty());
  }
  if (name == "records") {
    return std::make_unique<RecordsScreen>(textures_, font_, records_, contests_,
                                           story_progress(events_, quest, records_.unscored_events),
                                           from, resolve(records_.exit_to, place_before), frame_,
                                           credits_, quest, preview_world().world);
  }
  if (name == "battle_music") {
    return std::make_unique<BattleMusicScreen>(textures_, font_, battle_music_, from, frame_,
                                               credits_, quest);
  }
  if (name == "sonic_amp") {
    return std::make_unique<SonicAmpScreen>(textures_, font_, sonic_amp_, from, frame_, credits_,
                                            quest);
  }
  if (const auto used = used_sv_shops_.find(name); used != used_sv_shops_.end()) {
    return make_used_sv(name, used->second, from, quest);
  }
  if (name == "wild_machines_buy_chip") {
    return make_chip_shop(name, buy_chip_, from, quest);
  }
  if (name == "wild_machines_sell_chip") {
    return make_chip_shop(name, sell_chip_, from, quest);
  }
  if (name == "plustech_uplift") {
    return make_chip_shop(name, uplift_, from, quest);
  }
  if (name == "wild_machines_buy_module") {
    return make_module_shop(buy_module_, from, quest);
  }
  if (name == "wild_machines_sell_module") {
    return make_module_shop(sell_module_, from, quest);
  }
  if (name == "modules") {
    return make_module_shop(stock_module_, from, quest);
  }
  if (name == "sv_spec") {
    return make_sv_spec(from, place_before, quest);
  }
  if (name == "assemble") {
    return make_assemble(from, quest);
  }
  if (name == "hangar") {
    return make_hangar(from, quest);
  }
  if (name == "rumbling") {
    return make_rumbling(from, quest);
  }
  if (name == "keep_sv") {
    return make_keep_sv(from, quest);
  }
  if (name == "vs_mode") {
    return make_vs_mode(from, quest);
  }
  if (name == "plustech_customize") {
    TuneShop logic(customize_, quest, parts_, garage_, part_stats_);
    logic.set_full_refunds(full_refunds_);
    return std::make_unique<TuneShopScreen>(textures_, font_, std::move(logic), message_box_, from,
                                            frame_, credits_, quest, preview_world(),
                                            robot_preview_);
  }
  if (auto variant = make_shop_variant(name, from, quest)) {
    return variant;
  }
  if (dealer_.dealers.contains(name)) {
    return make_dealer(name, from, quest);
  }
  if (const auto found = placeholders_.find(name); found != placeholders_.end()) {
    return std::make_unique<PlaceholderScreen>(textures_.renderer(), found->second.label,
                                               found->second.music, placeholder_buttons_,
                                               resolve(found->second.return_to, place_before));
  }
  return nullptr;
}

} // namespace phantomcrash
