#pragma once

#include "phantomcrash/data_roots.hpp"
#include "phantomcrash/json.hpp"

#include <functional>
#include <map>
#include <string>
#include <utility>
#include <vector>

namespace phantomcrash {

// Shops that reuse another shop's screens under their own names
// (data/ui/shop_variants.json; a port addition): S.L.A.I.'s The Mechanist,
// Difference EG and Blood Music are Wild Machines and Sonic Amp in their own
// colours, each screen a document that extends the original's, and its
// junk shop is The Mechanist again with other stock.
enum class ShopVariantKind {
  location_menu,
  buy_module,
  sell_module,
  buy_chip,
  sell_chip,
  sonic_amp,
  used_sv
};

struct ShopVariant {
  ShopVariantKind kind{ShopVariantKind::location_menu};
  std::string document; // under data/, e.g. "ui/the_mechanist.json"
  // What a parts, CHIP or SV shop sells (shop_stock_named): "slai" or
  // "slai_junk"; empty for Phantom Crash's.
  std::string stock;
};

inline constexpr const char *shop_variants_document = "ui/shop_variants.json";

using ShopVariants = std::map<std::string, ShopVariant, std::less<>>; // by screen name

// Throws std::runtime_error naming an unknown kind.
[[nodiscard]] ShopVariants parse_shop_variants(const json::JsonValue &document);

// The location menus' screen names and documents: Phantom Crash's four and
// the shop variants'.
[[nodiscard]] std::vector<std::pair<std::string, std::string>>
location_menu_documents(const DataRoots &roots);

} // namespace phantomcrash
