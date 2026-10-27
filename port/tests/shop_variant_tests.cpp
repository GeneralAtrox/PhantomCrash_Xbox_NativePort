// S.L.A.I.'s shops: themed atlas art and the shop screens that reuse
// Phantom Crash's (data/ui/themes.json, data/ui/shop_variants.json).

#include "test_support.hpp"

#include <array>
#include <cstdint>
#include <string>
#include <vector>

#include "phantomcrash/chip_shop_script.hpp"
#include "phantomcrash/data_roots.hpp"
#include "phantomcrash/image_theme.hpp"
#include "phantomcrash/location_menu.hpp"
#include "phantomcrash/module_shop_script.hpp"
#include "phantomcrash/shop_variants.hpp"
#include "phantomcrash/sonic_amp_script.hpp"

using namespace phantomcrash;

namespace {

DataRoots shipped() {
  return DataRoots::shipped(PHANTOMCRASH_SOURCE_ROOT);
}

// A width x 1 strip of one RGBA colour.
std::vector<std::uint8_t> strip(const int width, const std::array<std::uint8_t, 4> &colour) {
  std::vector<std::uint8_t> pixels;
  for (int x = 0; x < width; ++x) {
    pixels.insert(pixels.end(), colour.begin(), colour.end());
  }
  return pixels;
}

} // namespace

TEST_CASE("themed image keys split at the last mark") {
  const auto themed = split_themed_image("textures/scroll/wild_tex/0.png#the_mechanist");
  CHECK(themed.image == "textures/scroll/wild_tex/0.png");
  CHECK(themed.theme == "the_mechanist");
  CHECK(split_themed_image("textures/scroll/wild_tex/0.png").theme.empty());
}

TEST_CASE("a theme recolours matching hues and leaves greys") {
  ImageTheme theme;
  theme.hues = {{25.0F, 71.0F}};
  theme.min_saturation = 0.08F;
  theme.hue = 212.0F;
  auto yellow = strip(2, {255, 211, 0, 255});
  apply_image_theme(theme, "a.png", yellow.data(), 2, 1, 8);
  CHECK(yellow[2] > yellow[0]); // blue now leads red
  CHECK(yellow[3] == 255);
  auto grey = strip(2, {128, 128, 128, 255});
  apply_image_theme(theme, "a.png", grey.data(), 2, 1, 8);
  CHECK(grey[0] == 128 && grey[1] == 128 && grey[2] == 128);
  // A lettering colour is themed as a texel of it is (a lit plate's erase fill).
  auto plate = strip(1, {0xff, 0xcc, 0x00, 255});
  apply_image_theme(theme, "a.png", plate.data(), 1, 1, 4);
  CHECK(themed_colour(theme, 0xffcc00U) ==
        ((std::uint32_t{plate[0]} << 16U) | (std::uint32_t{plate[1]} << 8U) | plate[2]));
  CHECK(themed_colour(theme, 0x808080U) == 0x808080U);
}

TEST_CASE("a theme recolours only its areas when it names some") {
  ImageTheme theme;
  theme.hues = {{25.0F, 71.0F}};
  theme.hue = 212.0F;
  theme.recolour = {{"b.png", {0, 0, 1, 1}}};
  auto pixels = strip(2, {255, 211, 0, 255});
  apply_image_theme(theme, "b.png", pixels.data(), 2, 1, 8);
  CHECK(pixels[2] > pixels[0]);
  CHECK(pixels[4] == 255 && pixels[5] == 211 && pixels[6] == 0);
  auto other = strip(2, {255, 211, 0, 255});
  apply_image_theme(theme, "c.png", other.data(), 2, 1, 8);
  CHECK(other[0] == 255 && other[2] == 0);
}

TEST_CASE("a theme erases a region from what surrounds it") {
  ImageTheme theme;
  theme.erase = {{"a.png", {1, 1, 3, 2}}};
  // Three black rows of four; the middle row has lettering and a lighter edge.
  std::vector<std::uint8_t> pixels = strip(4, {0, 0, 0, 255});
  for (int row = 0; row < 2; ++row) {
    const auto more = strip(4, {0, 0, 0, 255});
    pixels.insert(pixels.end(), more.begin(), more.end());
  }
  pixels[16 + 4] = pixels[16 + 8] = 200;
  pixels[16 + 12] = 90;
  apply_image_theme(theme, "a.png", pixels.data(), 4, 3, 16);
  CHECK(pixels[16 + 4] < 90 && pixels[16 + 8] < 90);
  CHECK(pixels[16 + 4] <= pixels[16 + 8]);
}

TEST_CASE("the shipped themes parse") {
  const auto themes = parse_image_themes(shipped().document("ui/themes.json"));
  REQUIRE(themes.contains("the_mechanist"));
  REQUIRE(themes.contains("blood_music"));
  REQUIRE(themes.contains("junk_shop"));
  REQUIRE(themes.contains("dealer_omsk"));
  CHECK(themes.at("dealer_omsk").paste.size() == 3);
  CHECK(themes.at("dealer_ma_fabrik").paste.front().clear);
  CHECK(themes.at("the_mechanist").paste.size() == 1);
  CHECK(themes.at("junk_shop").paste.empty()); // lettered instead (sprite_text.json)
  CHECK(themes.at("blood_music").recolour.size() == 1);
}

TEST_CASE("the shipped shop variants compose from the original screens") {
  const auto roots = shipped();
  const auto variants = parse_shop_variants(roots.document(shop_variants_document));
  REQUIRE(variants.size() == 12);
  for (const auto &[name, variant] : variants) {
    const auto document = roots.document(variant.document);
    CHECK(document.find("extends") == nullptr);
    // Each screen leads back to its shop's menu: "<shop>_<screen>".
    const std::string shop = name.substr(0, name.find("_", name.find("_") + 1));
    switch (variant.kind) {
    case ShopVariantKind::location_menu: {
      const auto menu = parse_location_menu(document, variant.document);
      CHECK(menu.items.size() >= 3);
      CHECK(menu.items.back().next == "town_map");
      for (std::size_t item = 0; item + 1 < menu.items.size(); ++item) {
        CHECK(menu.items[item].next.starts_with(name + "_"));
        CHECK(variants.contains(menu.items[item].next));
      }
      break;
    }
    case ShopVariantKind::buy_module:
      CHECK(variant.stock.starts_with("slai"));
      CHECK(parse_module_shop(document, ModuleShopMode::buy).stock_from[0] == 0);
      CHECK(parse_module_shop(document, ModuleShopMode::buy).exit_to == shop);
      break;
    case ShopVariantKind::sell_module:
      CHECK(variant.stock.empty());
      CHECK(parse_module_shop(document, ModuleShopMode::sell).exit_to == shop);
      break;
    case ShopVariantKind::used_sv:
      CHECK(variant.stock.starts_with("slai"));
      CHECK(document.string_or("exit_to", "") == shop);
      break;
    case ShopVariantKind::buy_chip:
    case ShopVariantKind::sell_chip:
      CHECK(parse_chip_shop(document, ChipShopMode::buy).exit_to == "difference_eg");
      break;
    case ShopVariantKind::sonic_amp: {
      const auto shop = parse_sonic_amp(document);
      CHECK(shop.grid.catalogue_ids.size() == 90);
      CHECK(shop.grid.catalogue_ids.front() == 81);
      break;
    }
    }
  }
  bool mechanist = false;
  for (const auto &[name, document] : location_menu_documents(roots)) {
    mechanist = mechanist || name == "the_mechanist";
  }
  CHECK(mechanist);
}
