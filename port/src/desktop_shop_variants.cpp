#include "desktop_game_screens.hpp"

#include "desktop_sonic_amp.hpp"
#include "phantomcrash/shop_variants.hpp"

#include <SDL3/SDL_log.h>

namespace phantomcrash {

void GameScreens::load_shop_variants(const DataRoots &roots, const BattleHudStyle &hud) {
  if (!roots.has_document(shop_variants_document)) {
    return;
  }
  for (const auto &[name, variant] : parse_shop_variants(roots.document(shop_variants_document))) {
    ShopStock stock = ShopStock::phantom_crash;
    if (!variant.stock.empty()) {
      if (const auto named = shop_stock_named(variant.stock)) {
        stock = *named;
      } else {
        SDL_Log("%s: unknown stock %s", name.c_str(), variant.stock.c_str());
      }
    }
    shop_stocks_.emplace(name, stock);
    switch (variant.kind) {
    case ShopVariantKind::location_menu:
      break; // the Screens own the location menus (location_menu_documents)
    case ShopVariantKind::buy_module:
    case ShopVariantKind::sell_module: {
      const auto mode =
          variant.kind == ShopVariantKind::buy_module ? ModuleShopMode::buy : ModuleShopMode::sell;
      module_shops_.emplace(name, parse_module_shop(roots.document(variant.document), mode));
      break;
    }
    case ShopVariantKind::buy_chip:
    case ShopVariantKind::sell_chip: {
      const auto mode =
          variant.kind == ShopVariantKind::buy_chip ? ChipShopMode::buy : ChipShopMode::sell;
      ChipShopScript script = parse_chip_shop(roots.document(variant.document), mode);
      use_reticle_sizes(script, hud);
      chip_shops_.emplace(name, std::move(script));
      break;
    }
    case ShopVariantKind::sonic_amp:
      sonic_amps_.emplace(name, parse_sonic_amp(roots.document(variant.document)));
      break;
    case ShopVariantKind::used_sv:
      used_sv_shops_.emplace(name,
                             UsedSvShop{parse_used_sv(roots.document(variant.document)), stock});
      break;
    }
  }
}

std::unique_ptr<Screen> GameScreens::make_shop_variant(const std::string &name,
                                                       const std::string &from,
                                                       const QuestSave &quest) const {
  const auto stocked = shop_stocks_.find(name);
  const ShopStock stock =
      stocked != shop_stocks_.end() ? stocked->second : ShopStock::phantom_crash;
  if (const auto shop = module_shops_.find(name); shop != module_shops_.end()) {
    return make_module_shop(shop->second, from, quest, stock);
  }
  if (const auto shop = chip_shops_.find(name); shop != chip_shops_.end()) {
    return make_chip_shop(name, shop->second, from, quest, stock);
  }
  if (const auto shop = sonic_amps_.find(name); shop != sonic_amps_.end()) {
    return std::make_unique<SonicAmpScreen>(textures_, font_, shop->second, from, frame_, credits_,
                                            quest);
  }
  return nullptr;
}

} // namespace phantomcrash
