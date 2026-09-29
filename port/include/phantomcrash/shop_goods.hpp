#pragma once

#include "phantomcrash/garage.hpp"
#include "phantomcrash/garage_tables.hpp"
#include "phantomcrash/part_stats.hpp"
#include "phantomcrash/robot_parts.hpp"

#include <cstdint>
#include <string>
#include <vector>

namespace phantomcrash {

// A line of the goods list: what it is, its name and the price asked (or
// paid, selling), and for the player's own its stock entry, the hangar it
// is fitted in, which of a pair of weapon slots (0 the first, 1 the second)
// and whether a hangar wears it (paints and emblems).
struct ShopGoods {
  Goods goods;
  std::string name;
  std::int64_t price{};
  int maker{no_maker};
  int stock{-1};
  int hangar{-1};
  int side{-1};
  bool in_use{};
};

// Which of a calendar day's seven module lineups (0x06d3a1) a kind's goods
// come from: bodies, legs, arm and backpack weapons, options, paints,
// stickers; -1 for CHIPs, sold on their own.
[[nodiscard]] int module_lineup_slot(GoodsKind kind);

// A category's goods on sale: its lineup (tables/garage.json
// module_lineups, 0x25a0d), those whose record exists.
[[nodiscard]] std::vector<ShopGoods>
goods_for_sale(const PartTables &parts, const GarageTables &garage, GoodsKind kind, int lineup);
// The player's goods of a kind (0x0182b1): the stock's parts and modules,
// the paints and emblems owned (not the ground colour or no emblem), by
// name; priced at what the shop pays.
[[nodiscard]] std::vector<ShopGoods> goods_owned(const PartStatsTable &table,
                                                 const PartTables &parts,
                                                 const GarageTables &garage, const Garage &owned,
                                                 GoodsKind kind, double sell_rate);

// A shop's goods list (0x06d1cd with 0x01733d's order): the goods, the row
// chosen on screen and the first row shown, and how they are sorted.
class GoodsList final {
public:
  // `flip` starts a sort by name A to Z and one by price highest first.
  void assign(std::vector<ShopGoods> goods, bool flip);
  // Moves `step` rows without wrapping, the list scrolling a row at a time
  // through `rows` shown; false when the chosen line stays.
  bool move(int step, int rows);
  // By name (0) or price (1); the same key again turns the order round, a
  // new key starts names A to Z and prices highest first.
  void sort(int key);
  // Reads again which hangar and side each of the player's lines is fitted
  // in and which paints and emblems are worn, keeping the order and cursor.
  void refit(const Garage &owned);

  [[nodiscard]] const std::vector<ShopGoods> &goods() const noexcept {
    return goods_;
  }
  [[nodiscard]] bool empty() const noexcept {
    return goods_.empty();
  }
  [[nodiscard]] int row() const noexcept {
    return row_;
  }
  [[nodiscard]] int scroll() const noexcept {
    return scroll_;
  }
  // The key sorted by (0 name, 1 price), and whether in the order it
  // starts in (names A to Z, prices highest first) rather than turned round.
  [[nodiscard]] int sort_key() const noexcept {
    return sort_key_;
  }
  [[nodiscard]] bool sort_fresh() const noexcept {
    return sort_flip_;
  }
  // The chosen line; nullptr when the list is empty.
  [[nodiscard]] const ShopGoods *chosen() const;
  [[nodiscard]] ShopGoods *chosen();

private:
  std::vector<ShopGoods> goods_;
  int row_{};
  int scroll_{};
  int sort_key_{};
  bool sort_flip_{};
};

} // namespace phantomcrash
