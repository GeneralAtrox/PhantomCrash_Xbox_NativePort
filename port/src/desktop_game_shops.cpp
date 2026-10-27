// GameScreens' shops: the dealers, the used SVs, the CHIP shops and the
// parts shops, each stocked from Phantom Crash's calendar or, for S.L.A.I.'s
// shops, from S.L.A.I.'s day and city (phantomcrash/slai_shops.hpp).

#include "desktop_game_screens.hpp"

#include "desktop_chip_shop.hpp"
#include "desktop_dealer.hpp"
#include "desktop_module_shop.hpp"
#include "desktop_used_sv.hpp"
#include "phantomcrash/added_chips.hpp"

#include <SDL3/SDL_log.h>

#include <limits>

namespace phantomcrash {

void GameScreens::load_slai_shops(const DataRoots &roots) {
  if (!roots.slai()) {
    return;
  }
  try {
    const auto table = read_converted_table(roots, slai_shops_file);
    if (!table) {
      SDL_Log("S.L.A.I. shops: %s is missing; register the S.L.A.I. assets again", slai_shops_file);
      return;
    }
    slai_shops_ = parse_slai_shops(*table);
  } catch (const std::exception &error) {
    SDL_Log("S.L.A.I. shops: %s unreadable: %s", slai_shops_file, error.what());
    return;
  }
  for (const auto &[name, dealer] : slai_shops_->dealers) {
    if (const auto look = dealer_.dealers.find(name);
        look != dealer_.dealers.end() && !dealer.profile.empty()) {
      look->second.profile.lines = dealer.profile;
    }
  }
}

std::optional<SlaiRotaEntry> GameScreens::slai_place(const QuestSave &quest) const {
  if (!slai_shops_ || !slai_rumble_) {
    return std::nullopt;
  }
  return slai_shop_place(slai_rumble_->rota, quest.day, quest.slai_rumble.city);
}

// A dealer's offers are its lineup on the day's calendar entry (0x06ce5d);
// S.L.A.I.'s dealers' on S.L.A.I.'s day, by their shop id (0x547950).
std::vector<SvOffer> GameScreens::dealer_lineup(const std::string &name,
                                                const QuestSave &quest) const {
  const auto look = dealer_.dealers.find(name);
  int lineup = -1;
  if (look == dealer_.dealers.end()) {
  } else if (look->second.slai_shop >= 0) {
    if (const auto place = slai_place(quest)) {
      lineup = slai_dealer_lineup(*slai_shops_, place->slai_day, look->second.slai_shop);
    }
  } else if (const int index = look->second.index;
             !contests_.calendar.empty() && index >= 0 && index < 3) {
    const auto days = static_cast<int>(contests_.calendar.size());
    lineup = contests_.calendar[static_cast<std::size_t>(quest.day % days)]
                 .dealers[static_cast<std::size_t>(index)];
  }
  return dealer_offers(garage_, parts_, lineup, dealer_.price_markup);
}

std::unique_ptr<Screen> GameScreens::make_dealer(const std::string &name, const std::string &from,
                                                 const QuestSave &quest) const {
  int &remembered = dealer_cursors_[name];
  const bool resume = from == "name_entry" || from == dealer_.purchase.spec_screen;
  Dealer logic(dealer_, name, dealer_lineup(name, quest), quest, parts_, garage_, from,
               resume ? remembered : 0);
  return std::make_unique<DealerScreen>(textures_, font_, std::move(logic), message_box_, from,
                                        frame_, credits_, quest, remembered, preview_world(),
                                        robot_preview_);
}

// Wild Machines' used SVs are the day's used_svs lineup (0x01ee3d). S.L.A.I.'s
// module shop sells the city-day row's SVs (0x5372d0) and its junk shop its
// junk row's; neither has Wild Machines' maker gate.
std::vector<SvOffer> GameScreens::used_sv_lineup(const UsedSvShop &shop,
                                                 const QuestSave &quest) const {
  int lineup = -1;
  if (shop.stock == ShopStock::phantom_crash) {
    if (!contests_.calendar.empty()) {
      const auto days = static_cast<int>(contests_.calendar.size());
      lineup = contests_.calendar[static_cast<std::size_t>(quest.day % days)].used_svs;
    }
    return used_sv_offers(shop.script, garage_, parts_, lineup, quest.won_contests);
  }
  if (const auto place = slai_place(quest); place && shop.stock == ShopStock::slai) {
    const auto *row = slai_shop_lineups(*slai_shops_, *place);
    lineup = row != nullptr ? row->used_svs : -1;
  } else if (place) {
    const auto *row = slai_junk_row(*slai_shops_, *place);
    lineup = row != nullptr ? slai_junk_svs(*slai_shops_, *row, quest.slai_rumble.stage) : -1;
  }
  UsedSvScript ungated = shop.script;
  ungated.gated_maker = std::numeric_limits<int>::max();
  return used_sv_offers(ungated, garage_, parts_, lineup, quest.won_contests);
}

std::unique_ptr<Screen> GameScreens::make_used_sv(const std::string &name, const UsedSvShop &shop,
                                                  const std::string &from,
                                                  const QuestSave &quest) const {
  int &remembered = dealer_cursors_[name];
  const bool resume = from == "name_entry" || from == shop.script.purchase.spec_screen;
  UsedSv logic(shop.script, used_sv_lineup(shop, quest), quest, parts_, garage_,
               resume ? remembered : 0);
  return std::make_unique<UsedSvScreen>(textures_, font_, std::move(logic), message_box_, from,
                                        frame_, credits_, quest, remembered, preview_world(),
                                        robot_preview_);
}

// BUY CHIP offers the day's chips lineup (0x01efdf) and the bosses' CHIPs the
// player has won that day (add_boss_chips), SELL CHIP and PlusTech's UPLIFT
// the player's CHIPs in stock; both resume at the CHIP chosen after NAME
// ENTRY. S.L.A.I.'s CHIP shop offers its city-day row's lineup (0x51c320).
std::unique_ptr<Screen> GameScreens::make_chip_shop(const std::string &name,
                                                    const ChipShopScript &script,
                                                    const std::string &from, const QuestSave &quest,
                                                    const ShopStock stock) const {
  std::vector<ChipOffer> offers;
  if (script.mode != ChipShopMode::buy) {
    offers = chips_in_stock(parts_, garage_, quest.garage);
  } else if (stock != ShopStock::phantom_crash) {
    const auto place = slai_place(quest);
    const auto *row = place ? slai_shop_lineups(*slai_shops_, *place) : nullptr;
    offers = chips_for_sale(parts_, garage_, row != nullptr ? row->chips : -1);
  } else if (!contests_.calendar.empty()) {
    const auto days = static_cast<int>(contests_.calendar.size());
    const int lineup = contests_.calendar[static_cast<std::size_t>(quest.day % days)].chips;
    offers = chips_for_sale(parts_, garage_, lineup);
    add_boss_chips(offers, parts_, garage_, contests_, quest.day, quest.won_contests);
  }
  ChipShopScript terms = script;
  if (full_refunds_ && script.mode == ChipShopMode::sell) {
    terms.sell_rate = 1.0; // Settings > Full Refunds: what each CHIP cost
    for (ChipOffer &offer : offers) {
      offer.price = quest.garage.stock.at(offer.stock).paid.value_or(offer.price);
    }
  }
  int &remembered = dealer_cursors_[name];
  ChipShop logic(terms, std::move(offers), quest, parts_, from == "name_entry" ? remembered : 0);
  logic.set_tuned_share(garage_.tuned_share);
  return std::make_unique<ChipShopScreen>(textures_, font_, std::move(logic), message_box_, from,
                                          frame_, credits_, quest, remembered);
}

// The day's lineup for each of the grid's categories but EXIT: Phantom
// Crash's calendar entry (0x06d3a1), S.L.A.I.'s city-day row (0x537480) or
// its junk row; -1 for none.
std::vector<int> GameScreens::module_lineups(const ModuleShopScript &script, const QuestSave &quest,
                                             const ShopStock stock) const {
  std::vector<int> lineups(script.categories.size(), -1);
  const std::array<int, slai_module_categories> *slai = nullptr;
  if (stock != ShopStock::phantom_crash) {
    if (const auto place = slai_place(quest); place && stock == ShopStock::slai) {
      const auto *row = slai_shop_lineups(*slai_shops_, *place);
      slai = row != nullptr ? &row->modules : nullptr;
    } else if (place) {
      const auto *row = slai_junk_row(*slai_shops_, *place);
      slai = row != nullptr ? &row->modules : nullptr;
    }
  }
  const auto days = static_cast<int>(contests_.calendar.size());
  for (std::size_t cell = 0; cell < script.categories.size(); ++cell) {
    const auto kind = script.categories[cell].kind;
    if (!kind) {
      continue;
    }
    const auto slot = static_cast<std::size_t>(module_lineup_slot(*kind));
    if (slai != nullptr) {
      lineups[cell] = slot < slai->size() ? (*slai)[slot] : -1;
    } else if (stock == ShopStock::phantom_crash && days > 0) {
      const auto &day = contests_.calendar[static_cast<std::size_t>(quest.day % days)];
      lineups[cell] = slot < day.modules.size() ? day.modules[slot] : -1;
    }
  }
  return lineups;
}

// With S.L.A.I.'s content, Phantom Crash's parts shop sells only its own
// records (S.L.A.I.'s deliveries join its lineups) and S.L.A.I.'s only
// S.L.A.I.'s.
ModuleShopScript GameScreens::stocked(const ModuleShopScript &script, const ShopStock stock) const {
  ModuleShopScript out = script;
  if (!slai_shops_ || script.mode != ModuleShopMode::buy) {
    return out;
  }
  for (const auto &[name, range] : slai_shops_->stock) {
    const auto kind = goods_kind_named(name);
    if (!kind) {
      continue;
    }
    const auto at = static_cast<std::size_t>(*kind);
    if (stock == ShopStock::phantom_crash) {
      out.stock_until[at] = std::min(out.stock_until[at], range.first);
    } else {
      out.stock_from[at] = std::max(out.stock_from[at], range.first);
      out.stock_until[at] = std::min(out.stock_until[at], range.end);
    }
  }
  return out;
}

// BUY MODULE offers each category's lineup for the day; SELL MODULE the
// player's own.
std::unique_ptr<Screen> GameScreens::make_module_shop(const ModuleShopScript &script,
                                                      const std::string &from,
                                                      const QuestSave &quest,
                                                      const ShopStock stock) const {
  std::vector<int> lineups;
  if (script.mode == ModuleShopMode::buy) {
    lineups = module_lineups(script, quest, stock);
  }
  ModuleShop logic(stocked(script, stock), quest, parts_, garage_, part_stats_, std::move(lineups));
  logic.set_looks(part_looks());
  logic.set_full_refunds(full_refunds_);
  return std::make_unique<ModuleShopScreen>(textures_, font_, std::move(logic), message_box_, from,
                                            frame_, credits_, quest, preview_world(),
                                            robot_preview_);
}

} // namespace phantomcrash
