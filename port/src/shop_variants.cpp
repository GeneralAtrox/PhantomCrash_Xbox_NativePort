#include "phantomcrash/shop_variants.hpp"

#include <array>
#include <stdexcept>
#include <string_view>
#include <utility>

namespace phantomcrash {
namespace {

constexpr std::array<std::pair<std::string_view, ShopVariantKind>, 7> kind_names{{
    {"location_menu", ShopVariantKind::location_menu},
    {"buy_module", ShopVariantKind::buy_module},
    {"sell_module", ShopVariantKind::sell_module},
    {"buy_chip", ShopVariantKind::buy_chip},
    {"sell_chip", ShopVariantKind::sell_chip},
    {"sonic_amp", ShopVariantKind::sonic_amp},
    {"used_sv", ShopVariantKind::used_sv},
}};

ShopVariantKind kind_named(const std::string &screen, const std::string &name) {
  for (const auto &[known, kind] : kind_names) {
    if (known == name) {
      return kind;
    }
  }
  throw std::runtime_error("shop_variants.json: " + screen + " has no kind " + name);
}

} // namespace

ShopVariants parse_shop_variants(const json::JsonValue &document) {
  ShopVariants variants;
  const json::JsonValue *screens = document.find("screens");
  if (screens == nullptr) {
    return variants;
  }
  for (const auto &[name, entry] : screens->object()) {
    variants.emplace(name,
                     ShopVariant{kind_named(name, entry.string_or("kind", "")),
                                 entry.string_or("document", ""), entry.string_or("stock", "")});
  }
  return variants;
}

std::vector<std::pair<std::string, std::string>> location_menu_documents(const DataRoots &roots) {
  std::vector<std::pair<std::string, std::string>> menus;
  for (const char *menu : {"club_wired", "garage", "wild_machines", "plustech"}) {
    menus.emplace_back(menu, "ui/" + std::string(menu) + ".json");
  }
  if (roots.has_document(shop_variants_document)) {
    for (auto &[name, variant] : parse_shop_variants(roots.document(shop_variants_document))) {
      if (variant.kind == ShopVariantKind::location_menu) {
        menus.emplace_back(name, std::move(variant.document));
      }
    }
  }
  return menus;
}

} // namespace phantomcrash
