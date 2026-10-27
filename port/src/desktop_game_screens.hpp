#pragma once

#include "desktop_frame.hpp"
#include "desktop_robot_preview.hpp"
#include "desktop_screen.hpp"
#include "desktop_textures.hpp"
#include "phantomcrash/data_roots.hpp"
#include "phantomcrash/slai_rumble.hpp"
#include "phantomcrash/slai_rumble_hub.hpp"
#include "phantomcrash/slai_shops.hpp"
#include "phantomcrash/contests.hpp"
#include "phantomcrash/glyph_font.hpp"
#include "phantomcrash/json.hpp"
#include "phantomcrash/assemble_script.hpp"
#include "phantomcrash/battle_music_script.hpp"
#include "phantomcrash/chip_shop_script.hpp"
#include "phantomcrash/dealer_script.hpp"
#include "phantomcrash/used_sv_script.hpp"
#include "phantomcrash/garage_tables.hpp"
#include "phantomcrash/hangar_menu_script.hpp"
#include "phantomcrash/keep_sv.hpp"
#include "phantomcrash/vs_mode.hpp"
#include "phantomcrash/message_box.hpp"
#include "phantomcrash/module_shop_script.hpp"
#include "phantomcrash/tune_shop_script.hpp"
#include "phantomcrash/part_stats.hpp"
#include "phantomcrash/robot_parts.hpp"
#include "phantomcrash/sonic_amp_script.hpp"
#include "phantomcrash/sv_purchase.hpp"
#include "phantomcrash/sv_spec_script.hpp"
#include "phantomcrash/records_script.hpp"
#include "phantomcrash/rumbling_rules.hpp"
#include "phantomcrash/rumbling_script.hpp"
#include "phantomcrash/schedule.hpp"
#include "phantomcrash/sleep.hpp"
#include "phantomcrash/story_event.hpp"
#include "phantomcrash/vs_scoobee.hpp"

#include <functional>
#include <map>
#include <memory>
#include <optional>
#include <string>
#include <utility>
#include <vector>

namespace phantomcrash {

// A table the conversion writes (tables/...), or none (logged) when the
// conversion lacks it.
[[nodiscard]] std::optional<json::JsonValue> converted_table(const DataRoots &roots,
                                                             const char *relative);

// The screens of the game proper that the town's menus lead to (the
// garage's, Club Wired's and the shops'), and a placeholder from
// data/ui/placeholders.json for each one not ported yet.
class GameScreens final {
public:
  // The story events (for RECORDS' achievement rate) outlive the screens.
  GameScreens(TextureCache &textures, const DataRoots &roots, const GlyphFont &font,
              const FrameStyle &frame, const std::vector<MusicCredit> &credits,
              const std::vector<StoryEvent> &events);

  // The screen named `name`, opened from `from`; `place_before` is the
  // place the player was at before any story event, where a placeholder
  // that returns "back" goes. Null when `name` is none of these.
  [[nodiscard]] std::unique_ptr<Screen> make(const std::string &name, const std::string &from,
                                             const std::string &place_before,
                                             const QuestSave &quest) const;

  [[nodiscard]] const PartTables &part_tables() const noexcept {
    return parts_;
  }
  [[nodiscard]] const GarageTables &garage_tables() const noexcept {
    return garage_;
  }
  [[nodiscard]] const ContestTables &contests() const noexcept {
    return contests_;
  }
  [[nodiscard]] RegulationCheck regulation_check() const noexcept {
    return {rumbling_rules_, parts_, garage_, part_stats_, open_contests_};
  }
  [[nodiscard]] const SleepScript &sleep_script() const noexcept {
    return sleep_;
  }
  // S.L.A.I. Rumble's tables, while S.L.A.I. is installed and enabled.
  [[nodiscard]] const SlaiRumbleData *slai_rumble() const noexcept {
    return slai_rumble_ ? &*slai_rumble_ : nullptr;
  }
  // Tools > Cheats: RUMBLING lets the player into any contest held.
  void set_open_contests(const bool open) noexcept {
    open_contests_ = open;
  }
  // The live RUMBLING's robots an area (settings "rumble_robots"; 0 the
  // original's roster size).
  void set_rumble_robots(const int robots) noexcept {
    rumbling_rules_.roster.live_size = robots;
  }

  // The Scoobees kept for the VS mode, as KEEP SV leaves them.
  [[nodiscard]] const std::vector<VsScoobee> &vs_scoobees() const noexcept {
    return vs_scoobees_;
  }
  void set_vs_scoobees(std::vector<VsScoobee> scoobees) {
    vs_scoobees_ = std::move(scoobees);
  }
  // Who entered the VS mode and with what, for its battle.
  [[nodiscard]] const VsSession &vs_session() const noexcept {
    return vs_session_;
  }
  // The VS battle keeps its tallies and its menu's way back here.
  [[nodiscard]] VsSession &vs_session() noexcept {
    return vs_session_;
  }
  // The hangar whose Scoobee the hangar's KEEP chose, for KEEP SV to keep.
  void set_kept_hangar(const int hangar) noexcept {
    kept_hangar_ = hangar;
  }
  // Where the garage's robot previews get the 3D from when they open.
  void set_preview_world(std::function<PreviewWorld()> world) {
    preview_world_ = std::move(world);
  }

private:
  // A used SV screen: Wild Machines' or one of S.L.A.I.'s, and its stock.
  struct UsedSvShop {
    UsedSvScript script;
    ShopStock stock{ShopStock::phantom_crash};
  };

  // The shops (desktop_game_shops.cpp).
  void load_slai_shops(const DataRoots &roots);
  // Where S.L.A.I.'s shops stand today; nothing without S.L.A.I.'s tables.
  [[nodiscard]] std::optional<SlaiRotaEntry> slai_place(const QuestSave &quest) const;
  [[nodiscard]] std::unique_ptr<Screen> make_used_sv(const std::string &name,
                                                     const UsedSvShop &shop,
                                                     const std::string &from,
                                                     const QuestSave &quest) const;
  [[nodiscard]] std::unique_ptr<Screen>
  make_chip_shop(const std::string &name, const ChipShopScript &script, const std::string &from,
                 const QuestSave &quest, ShopStock stock = ShopStock::phantom_crash) const;
  [[nodiscard]] std::unique_ptr<Screen>
  make_module_shop(const ModuleShopScript &script, const std::string &from, const QuestSave &quest,
                   ShopStock stock = ShopStock::phantom_crash) const;
  [[nodiscard]] std::vector<int> module_lineups(const ModuleShopScript &script,
                                                const QuestSave &quest, ShopStock stock) const;
  // The script with its records limited to the stock's (Phantom Crash's
  // without S.L.A.I.'s, or S.L.A.I.'s).
  [[nodiscard]] ModuleShopScript stocked(const ModuleShopScript &script, ShopStock stock) const;
  [[nodiscard]] std::unique_ptr<Screen> make_hangar(const std::string &from,
                                                    const QuestSave &quest) const;
  [[nodiscard]] std::unique_ptr<Screen> make_assemble(const std::string &from,
                                                      const QuestSave &quest) const;
  [[nodiscard]] std::unique_ptr<Screen>
  make_dealer(const std::string &name, const std::string &from, const QuestSave &quest) const;
  [[nodiscard]] std::unique_ptr<Screen> make_rumbling(const std::string &from,
                                                      const QuestSave &quest) const;
  // S.L.A.I.'s shops (phantomcrash/shop_variants.hpp; desktop_shop_variants.cpp).
  void load_shop_variants(const DataRoots &roots, const BattleHudStyle &hud);
  [[nodiscard]] std::unique_ptr<Screen>
  make_shop_variant(const std::string &name, const std::string &from, const QuestSave &quest) const;
  [[nodiscard]] std::unique_ptr<Screen> make_keep_sv(const std::string &from,
                                                     const QuestSave &quest) const;
  [[nodiscard]] std::unique_ptr<Screen> make_vs_mode(const std::string &from,
                                                     const QuestSave &quest) const;
  [[nodiscard]] std::unique_ptr<Screen> make_sv_spec(const std::string &from,
                                                     const std::string &place_before,
                                                     const QuestSave &quest) const;
  [[nodiscard]] PreviewWorld preview_world() const {
    return preview_world_ ? preview_world_() : PreviewWorld{};
  }
  // The meshes each part may wear (part_looks.hpp); none without the 3D.
  [[nodiscard]] PartLooks part_looks() const;
  // The SVs a dealer or a used SV shop offers on the day.
  [[nodiscard]] std::vector<SvOffer> dealer_lineup(const std::string &name,
                                                   const QuestSave &quest) const;
  [[nodiscard]] std::vector<SvOffer> used_sv_lineup(const UsedSvShop &shop,
                                                    const QuestSave &quest) const;
  // Where "back" leads: the place before any story event.
  [[nodiscard]] std::string resolve(const std::string &target,
                                    const std::string &place_before) const;

  struct Placeholder {
    std::string label;
    std::string music;
    std::string return_to;
  };

  TextureCache &textures_;
  const GlyphFont &font_;
  const FrameStyle &frame_;
  const std::vector<MusicCredit> &credits_;
  const std::vector<StoryEvent> &events_;
  ContestTables contests_;
  std::optional<SlaiRumbleData> slai_rumble_;
  // With S.L.A.I.: Rumble S.L.A.I. (ui/slai_rumble.json) and Battle School
  // (ui/slai_battle_school.json), by screen name.
  std::map<std::string, SlaiRumbleHubScript, std::less<>> slai_hubs_;
  SleepScript sleep_;
  bool open_contests_{};
  ScheduleScript schedule_;
  RecordsScript records_;
  BattleMusicScript battle_music_;
  SonicAmpScript sonic_amp_;
  DealerScript dealer_;
  UsedSvScript used_sv_;
  ChipShopScript buy_chip_;
  ChipShopScript sell_chip_;
  ChipShopScript uplift_;
  ModuleShopScript buy_module_;
  ModuleShopScript sell_module_;
  ModuleShopScript stock_module_; // the garage's MODULES
  // The shop variants' screens, by screen name.
  std::map<std::string, ModuleShopScript, std::less<>> module_shops_;
  std::map<std::string, ChipShopScript, std::less<>> chip_shops_;
  std::map<std::string, SonicAmpScript, std::less<>> sonic_amps_;
  // What each variant's shop sells (its "stock"), by screen name.
  std::map<std::string, ShopStock, std::less<>> shop_stocks_;
  // The used SV screens, Wild Machines' and the variants', by screen name.
  std::map<std::string, UsedSvShop, std::less<>> used_sv_shops_;
  // With S.L.A.I.: what its shops sell, by day and city.
  std::optional<SlaiShops> slai_shops_;
  TuneShopScript customize_;
  HangarMenuScript hangar_;
  AssembleScript assemble_;
  SvSpecScript sv_spec_;
  RobotPreviewScript robot_preview_;
  PartLookRules part_look_rules_;
  std::function<PreviewWorld()> preview_world_;
  RumblingScript rumbling_;
  RumblingRules rumbling_rules_;
  PartStatsTable part_stats_;
  MessageBoxStyle message_box_;
  PartTables parts_;
  GarageTables garage_;
  // Each dealer's menu item, to resume at from SV SPEC and NAME ENTRY, and
  // the hangar chosen, to resume at from the screens its menu opens.
  mutable std::map<std::string, int> dealer_cursors_;
  // Where the hangar's EXIT leads: the garage, or RUMBLING when SELECT SV
  // opened it.
  mutable std::string hangar_exit_;
  KeepSvScript keep_sv_;
  mutable std::vector<VsScoobee> vs_scoobees_;
  mutable KeepSvCursor keep_cursor_;
  mutable int kept_hangar_{-1};
  VsModeScript vs_mode_;
  mutable VsSession vs_session_;
  std::map<std::string, Placeholder> placeholders_;
  Buttons placeholder_buttons_{};
  std::string placeholder_return_;
};

} // namespace phantomcrash
