// S.L.A.I.'s shops' stock (phantomcrash/slai_shops.hpp).

#include "phantomcrash/slai_shops.hpp"

#include "phantomcrash/json_fields.hpp"

#include <algorithm>
#include <stdexcept>

namespace phantomcrash {
namespace {

using json::JsonValue;
using json::required;

constexpr int cycle_days = 10; // the junk shop's cycle (0x51c5a0)

int integer(const JsonValue &value, const char *key, int fallback = -1) {
  return static_cast<int>(value.number_or(key, fallback));
}

std::vector<int> integers(const JsonValue &value) {
  std::vector<int> out;
  for (const auto &item : value.array()) {
    out.push_back(static_cast<int>(item.number()));
  }
  return out;
}

std::array<int, slai_module_categories> categories(const JsonValue &value) {
  const auto values = integers(required(value, "modules"));
  if (values.size() != slai_module_categories) {
    throw std::runtime_error("modules: one lineup a category");
  }
  std::array<int, slai_module_categories> out{};
  std::ranges::copy(values, out.begin());
  return out;
}

template <typename Value> const Value *at(const std::vector<Value> &values, const int index) {
  return index >= 0 && index < static_cast<int>(values.size())
             ? &values[static_cast<std::size_t>(index)]
             : nullptr;
}

void parse_junk(const JsonValue &junk, SlaiShops &shops) {
  shops.junk_cycle = integers(required(junk, "cycle"));
  for (const auto &row : required(junk, "rows").array()) {
    shops.junk_rows.push_back({categories(row), integer(row, "sv_row")});
  }
  shops.junk_sv_lineups = integers(required(junk, "sv_lineups"));
  if (const auto *locked = junk.find("locked_sv_rows"); locked != nullptr) {
    for (const auto &[row, shown] : locked->object()) {
      shops.locked_sv_rows[std::stoi(row)] = static_cast<int>(shown.number());
    }
  }
  shops.unlock_stage = integer(junk, "unlock_stage", 0);
}

} // namespace

std::optional<ShopStock> shop_stock_named(const std::string_view name) {
  if (name == "phantom_crash") {
    return ShopStock::phantom_crash;
  }
  if (name == "slai") {
    return ShopStock::slai;
  }
  if (name == "slai_junk") {
    return ShopStock::slai_junk;
  }
  return std::nullopt;
}

SlaiShops parse_slai_shops(const JsonValue &document) {
  SlaiShops shops;
  for (const auto &day : required(document, "days").array()) {
    shops.day_dealers.push_back(integers(required(day, "dealers")));
    shops.day_city_days.push_back(integers(required(day, "city_days")));
  }
  for (const auto &row : required(document, "city_days").array()) {
    shops.city_days.push_back({integer(row, "chips"), categories(row), integer(row, "used_svs")});
    shops.junk_of_city_day.push_back(integer(row, "junk"));
  }
  parse_junk(required(document, "junk"), shops);
  if (const auto *dealers = document.find("dealers"); dealers != nullptr) {
    for (const auto &[name, dealer] : dealers->object()) {
      SlaiShopDealer out{integer(dealer, "shop"), {}};
      if (const auto *profile = dealer.find("profile"); profile != nullptr) {
        for (const auto &line : profile->array()) {
          out.profile.push_back(line.string());
        }
      }
      shops.dealers.emplace(name, std::move(out));
    }
  }
  if (const auto *stock = document.find("slai_stock"); stock != nullptr) {
    for (const auto &[kind, range] : stock->object()) {
      const auto ends = integers(range);
      if (ends.size() != 2) {
        throw std::runtime_error("slai_stock." + kind + ": [first, end]");
      }
      shops.stock[kind] = {ends[0], ends[1]};
    }
  }
  return shops;
}

std::optional<SlaiRotaEntry> slai_shop_place(const SlaiRota &rota, const int day, const int city) {
  const auto &open = slai_rota_day(rota, day);
  if (open.empty()) {
    return std::nullopt;
  }
  for (const SlaiRotaEntry &entry : open) {
    if (entry.city == city) {
      return entry;
    }
  }
  return open.front();
}

const SlaiShopLineups *slai_shop_lineups(const SlaiShops &shops, const SlaiRotaEntry &place) {
  const auto *rows = at(shops.day_city_days, place.slai_day);
  const int *row = rows != nullptr ? at(*rows, place.city) : nullptr;
  return row != nullptr ? at(shops.city_days, *row) : nullptr;
}

const SlaiJunkRow *slai_junk_row(const SlaiShops &shops, const SlaiRotaEntry &place) {
  const int *offset = at(shops.junk_cycle, place.city);
  if (offset == nullptr) {
    return nullptr;
  }
  const int day = place.slai_day / cycle_days * cycle_days + *offset - 1;
  const auto *rows = at(shops.day_city_days, day);
  const int *row = rows != nullptr ? at(*rows, place.city) : nullptr;
  const int *junk = row != nullptr ? at(shops.junk_of_city_day, *row) : nullptr;
  return junk != nullptr ? at(shops.junk_rows, *junk) : nullptr;
}

int slai_junk_svs(const SlaiShops &shops, const SlaiJunkRow &row, const int stage) {
  int sv_row = row.sv_row;
  if (stage < shops.unlock_stage) {
    if (const auto locked = shops.locked_sv_rows.find(sv_row);
        locked != shops.locked_sv_rows.end()) {
      sv_row = locked->second;
    }
  }
  const int *lineup = at(shops.junk_sv_lineups, sv_row);
  return lineup != nullptr ? *lineup : -1;
}

int slai_dealer_lineup(const SlaiShops &shops, const int slai_day, const int shop) {
  const auto *lineups = at(shops.day_dealers, slai_day);
  const int *lineup = lineups != nullptr ? at(*lineups, shop) : nullptr;
  return lineup != nullptr ? *lineup : -1;
}

} // namespace phantomcrash
