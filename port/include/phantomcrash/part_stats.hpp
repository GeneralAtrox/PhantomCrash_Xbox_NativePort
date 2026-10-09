#pragma once

#include "phantomcrash/garage.hpp"
#include "phantomcrash/garage_tables.hpp"
#include "phantomcrash/json.hpp"
#include "phantomcrash/robot_parts.hpp"
#include "phantomcrash/sprite_layer.hpp"

#include <array>
#include <cstdint>
#include <optional>
#include <string>
#include <string_view>
#include <vector>

namespace phantomcrash {

// What a shop deals in, by the original's part kind (0x03cbc3's +2):
// the stock's parts and modules, then paints and emblems.
enum class GoodsKind { body, legs, arm, shoulder, chip, module, paint, emblem };
inline constexpr int goods_kind_count = 8;

// The stock kind for parts and modules; none for paints and emblems.
[[nodiscard]] std::optional<PartKind> stock_kind(GoodsKind kind);
// "body", "legs", "arm", "shoulder", "chip", "module", "paint", "emblem".
[[nodiscard]] std::optional<GoodsKind> goods_kind_named(std::string_view name);

// A thing a shop shows: its kind, record and level.
struct Goods {
  GoodsKind kind{GoodsKind::body};
  int id{};
  int level{};
};

// The maker a part's record names (0x01ede8, +4): 0 Holy to 5 Charga;
// `no_maker` for modules, paints and emblems.
inline constexpr int no_maker = 7;
[[nodiscard]] int goods_maker(const PartTables &parts, const Goods &goods);
[[nodiscard]] std::string goods_name(const PartTables &parts, const GarageTables &garage,
                                     const Goods &goods);
// Its price (0x0932c7); 0 for a record missing.
[[nodiscard]] std::int64_t goods_price(const PartTables &parts, const GarageTables &garage,
                                       const Goods &goods);
// A module that cuts beam damage shows LASER REDUCTION instead of its O.C
// times (0x0bb6e8 above 0).
[[nodiscard]] bool reduces_lasers(const GarageTables &garage, const Goods &goods);

// A line of the stats panel (0x03cbc3), each a step below the last.
enum class StatRow {
  level,           // the level, beside HEAVY, LIGHT or NORMAL
  weight,          // tonnes
  capacity,        // legs: the load carried before slowing, tonnes
  armour,          // DURABILITY
  sockets,         // bodies: SOCKET HOLE
  speed,           // legs: BASE SPEED
  jump,            // legs: JUMP HEIGHT
  ammo,            // AMMO/ENERGY
  rounds,          // FIRING
  recharge,        // E CHARGE TIME, seconds
  damage,          // DAMAGE
  module_sockets,  // modules: USE SOCKET HOLE
  active_time,     // modules: O.C ACTIVE TIME, seconds
  charge_time,     // modules: O.C CHARGE TIME, seconds
  laser_reduction, // modules: LASER REDUCTION, percent
};
inline constexpr int stat_row_count = 15;

// A stat the panel's art has no line for, shown below its lines only when
// the goods change it (data/ui/part_stats.json "extras"): its label, unit
// and where the value comes from.
enum class ExtraStatSource {
  critical_percent,     // modules and CHIPs: MORTAL SHOT
  dodge_percent,        // CHIPs
  knockdown_resistance, // modules and CHIPs
  knockback_resistance, // legs
};

struct ExtraStat {
  ExtraStatSource source{ExtraStatSource::critical_percent};
  std::string label;
  std::string unit;
  int decimals{};
};

// Which lines each kind shows and how they read (data/ui/part_stats.json).
struct PartStatsTable {
  std::array<std::vector<StatRow>, goods_kind_count> rows;
  std::array<int, stat_row_count> decimals{};
  float hundredths{0.01F};     // weights and loads are kept in hundredths
  float battle_rate{30.0F};    // a beam's recharge counts battle ticks
  float display_rate{60.0F};   // a module's O.C times count display frames
  std::vector<int> full_price; // modules bought back at their whole price
  std::array<std::vector<ExtraStat>, goods_kind_count> extras;
};

// Throws std::runtime_error naming what is malformed.
[[nodiscard]] PartStatsTable parse_part_stats(const json::JsonValue &document);

// A line's value as shown, with its digits after the point.
struct StatLine {
  StatRow row{StatRow::level};
  double value{};
  int decimals{};
};

// The lines for `goods`: a module shows its O.C times or its laser
// reduction, not both; armour is never below the player's floor.
[[nodiscard]] std::vector<StatLine> part_stats(const PartStatsTable &table, const PartTables &parts,
                                               const GarageTables &garage, const Goods &goods);

// An extra line as shown: its label, value and unit.
struct ExtraStatLine {
  std::string label;
  std::string unit;
  std::string value;
};

// The extras of part_stats.json; none when it lists none.
[[nodiscard]] std::array<std::vector<ExtraStat>, goods_kind_count>
parse_extra_stats(const json::JsonValue &document);
// The goods' extra lines, leaving out those at 0.
[[nodiscard]] std::vector<ExtraStatLine> extra_stats(const PartStatsTable &table,
                                                     const PartTables &parts,
                                                     const GarageTables &garage,
                                                     const Goods &goods);

// What a shop pays for goods: `rate` of the price, or all of it for the
// modules the table names (0x22ea6: the Gold Bar).
[[nodiscard]] std::int64_t buy_back_price(const PartStatsTable &table, const PartTables &parts,
                                          const GarageTables &garage, const Goods &goods,
                                          double rate);

// Where the panel's values go (0x03cbc3): right-aligned at `right`, a
// `step` apart; the level's word sits `sign_offset` left of the column.
struct PartStatsStyle {
  float right{};
  float step{};
  int layer{};
  std::uint32_t colour{0xffffff};
  float sign_offset{};
  std::array<Sprite, 3> signs; // LIGHT (below 0), NORMAL (0), HEAVY (above 0)
};

[[nodiscard]] PartStatsStyle parse_part_stats_style(const json::JsonValue &value);

// "12", "0.250", "-0.2": a value with its decimals, as the original writes
// them right to left (0x0bc769).
[[nodiscard]] std::string stat_text(const StatLine &line);

} // namespace phantomcrash
