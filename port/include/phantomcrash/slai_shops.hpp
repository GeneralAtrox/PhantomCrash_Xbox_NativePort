#pragma once

// S.L.A.I.'s shops' stock (tables/slai_shops.json, written by
// slai/shop_table.py; docs/SLAI_INTEGRATION.md, "S.L.A.I.'s shops"). The
// registration appends S.L.A.I.'s CHIPs, SVs and lineups to the pack's
// tables; this table names, by S.L.A.I. day and city, which of those
// lineups each of S.L.A.I.'s shops shows, and which records are S.L.A.I.'s,
// so Phantom Crash's own shops leave them out.

#include "phantomcrash/json.hpp"
#include "phantomcrash/slai_rota.hpp"

#include <array>
#include <functional>
#include <map>
#include <optional>
#include <string>
#include <string_view>
#include <vector>

namespace phantomcrash {

inline constexpr const char *slai_shops_file = "tables/slai_shops.json";
// S.L.A.I.'s module shop categories: bodies, legs, arms, shoulders,
// options, paints, emblems (0x537480), the port's module_lineup_slot order.
inline constexpr int slai_module_categories = 7;

// What a shop sells: Phantom Crash's stock by its calendar (without
// S.L.A.I.'s records), or S.L.A.I.'s by its day and city, at its module
// shop or its junk shop.
enum class ShopStock { phantom_crash, slai, slai_junk };

// "phantom_crash", "slai" or "slai_junk"; nothing for another name.
[[nodiscard]] std::optional<ShopStock> shop_stock_named(std::string_view name);

// A city-day row's shops, as garage lineups (-1 none).
struct SlaiShopLineups {
  int chips{-1};                                   // +0x28
  std::array<int, slai_module_categories> modules; // +0x2a..+0x36; options none
  int used_svs{-1};                                // +0x38
};

struct SlaiJunkRow {
  std::array<int, slai_module_categories> modules; // garage module lineups
  int sv_row{-1};                                  // +0xe: a row of junk SVs
};

struct SlaiShopDealer {
  int shop{-1};                     // 0x547950's shop id: the day row's dealer lineup
  std::vector<std::string> profile; // its company profile, from the disc
};

struct SlaiStockRange {
  int first{};
  int end{};

  [[nodiscard]] bool holds(int id) const noexcept {
    return id >= first && id < end;
  }
};

struct SlaiShops {
  std::vector<std::vector<int>> day_dealers;   // by S.L.A.I. day, by shop id
  std::vector<std::vector<int>> day_city_days; // by S.L.A.I. day, by city
  std::vector<SlaiShopLineups> city_days;
  std::vector<int> junk_of_city_day; // the junk row each city-day row names (+0x3a)
  std::vector<int> junk_cycle;       // by city: the day in each ten (0x43a908)
  std::vector<SlaiJunkRow> junk_rows;
  std::vector<int> junk_sv_lineups;  // by SV row: a garage used SV lineup
  std::map<int, int> locked_sv_rows; // SV rows shown instead until unlock_stage
  int unlock_stage{};
  std::map<std::string, SlaiShopDealer, std::less<>> dealers; // by dealer screen
  // S.L.A.I.'s records by goods kind name (body, legs, arm, shoulder,
  // paint, emblem) and "chip".
  std::map<std::string, SlaiStockRange, std::less<>> stock;
};

// Throws std::runtime_error naming the member at fault.
[[nodiscard]] SlaiShops parse_slai_shops(const json::JsonValue &document);

// Where S.L.A.I.'s shops stand on calendar day `day`: the player's city
// when the rota opens it that day, else the day's first open city; nothing
// on a day with none.
[[nodiscard]] std::optional<SlaiRotaEntry> slai_shop_place(const SlaiRota &rota, int day, int city);

// The place's city-day row's lineups; null outside the table.
[[nodiscard]] const SlaiShopLineups *slai_shop_lineups(const SlaiShops &shops,
                                                       const SlaiRotaEntry &place);
// The junk shop's row: the one named by the city's city-day row on the day
// of the ten-day cycle it shows (0x51c5a0); null for none.
[[nodiscard]] const SlaiJunkRow *slai_junk_row(const SlaiShops &shops, const SlaiRotaEntry &place);
// Its SVs' garage lineup: the best rows hold others until the stage ladder
// reaches unlock_stage (0x51c620, the flag 0x25a7f0 sets); -1 for none.
[[nodiscard]] int slai_junk_svs(const SlaiShops &shops, const SlaiJunkRow &row, int stage);
// A dealer's garage lineup on an S.L.A.I. day; -1 for none.
[[nodiscard]] int slai_dealer_lineup(const SlaiShops &shops, int slai_day, int shop);

} // namespace phantomcrash
