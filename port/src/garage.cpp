#include "phantomcrash/garage.hpp"

#include <algorithm>
#include <cstdint>
#include <utility>

namespace phantomcrash {
namespace {

using json::JsonValue;

constexpr std::array<const char *, 6> kind_names{"body",     "legs", "arm",
                                                 "shoulder", "chip", "module"};

int integer_or(const JsonValue &value, const char *key, const int fallback) {
  return static_cast<int>(value.number_or(key, fallback));
}

JsonValue number(const int value) {
  return JsonValue{static_cast<std::int64_t>(value)};
}

JsonValue numbers(const auto &values) {
  JsonValue::Array out;
  for (const int value : values) {
    out.push_back(number(value));
  }
  return JsonValue{std::move(out)};
}

std::vector<int> read_numbers(const JsonValue &value, const char *key) {
  std::vector<int> out;
  if (const auto *list = value.find(key); list != nullptr && list->is_array()) {
    for (const auto &entry : list->array()) {
      if (entry.is_number()) {
        out.push_back(static_cast<int>(entry.number()));
      }
    }
  }
  return out;
}

StockItem stock_from_json(const JsonValue &value) {
  StockItem item;
  const std::string kind = value.string_or("kind", "body");
  for (std::size_t k = 0; k < kind_names.size(); ++k) {
    if (kind == kind_names[k]) {
      item.kind = static_cast<PartKind>(k);
    }
  }
  item.id = integer_or(value, "id", 0);
  item.level = integer_or(value, "level", 0);
  item.name = value.string_or("name", "");
  item.hangar = integer_or(value, "hangar", -1);
  item.look = integer_or(value, "look", -1);
  if (const auto *paid = value.find("paid"); paid != nullptr && paid->is_number()) {
    item.paid = std::max<std::int64_t>(0, static_cast<std::int64_t>(paid->number()));
  }
  return item;
}

Hangar hangar_from_json(const JsonValue &value, const std::map<int, StockItem> &stock) {
  Hangar hangar;
  hangar.owned = value.boolean_or("owned", false);
  const auto in_stock = [&stock](const int index) { return stock.contains(index); };
  const std::vector<int> parts = read_numbers(value, "parts");
  for (std::size_t slot = 0; slot < parts.size() && slot < hangar.parts.size(); ++slot) {
    hangar.parts[slot] = in_stock(parts[slot]) ? parts[slot] : -1;
  }
  for (const int module : read_numbers(value, "modules")) {
    if (in_stock(module) && hangar.modules.size() < static_cast<std::size_t>(robot_module_slots)) {
      hangar.modules.push_back(module);
    }
  }
  hangar.name = value.string_or("name", "");
  hangar.paint = integer_or(value, "paint", 0);
  hangar.emblem = integer_or(value, "emblem", emblem_none);
  return hangar;
}

void read_stock(const JsonValue &value, const int size, std::map<int, StockItem> &stock) {
  if (const auto *list = value.find("stock"); list != nullptr && list->is_array()) {
    for (const auto &entry : list->array()) {
      const int index = integer_or(entry, "index", -1);
      if (index >= 0 && index < size) {
        stock[index] = stock_from_json(entry);
      }
    }
  }
}

JsonValue stock_to_json(const std::map<int, StockItem> &stock) {
  JsonValue::Array out;
  for (const auto &[index, item] : stock) {
    JsonValue::Object entry{
        {"index", number(index)},
        {"kind", JsonValue{std::string{kind_names[static_cast<std::size_t>(item.kind)]}}},
        {"id", number(item.id)},
        {"level", number(item.level)},
        {"name", JsonValue{item.name}},
        {"hangar", number(item.hangar)},
    };
    if (item.look >= 0) {
      entry.emplace_back("look", number(item.look));
    }
    if (item.paid) {
      entry.emplace_back("paid", JsonValue{*item.paid});
    }
    out.push_back(JsonValue{std::move(entry)});
  }
  return JsonValue{std::move(out)};
}

JsonValue::Object hangar_to_json(const Hangar &hangar) {
  return {
      {"owned", JsonValue{hangar.owned}},   {"parts", numbers(hangar.parts)},
      {"modules", numbers(hangar.modules)}, {"name", JsonValue{hangar.name}},
      {"paint", number(hangar.paint)},      {"emblem", number(hangar.emblem)},
  };
}

SetAside set_aside_from_json(const JsonValue &value, const int size) {
  SetAside set_aside;
  read_stock(value, size, set_aside.stock);
  if (const auto *hangars = value.find("hangars"); hangars != nullptr && hangars->is_array()) {
    for (const auto &entry : hangars->array()) {
      const int index = integer_or(entry, "index", -1);
      if (index >= 0 && index < garage_hangar_count) {
        set_aside.hangars[index] = hangar_from_json(entry, set_aside.stock);
      }
    }
  }
  set_aside.riding = integer_or(value, "riding", -1);
  set_aside.paints = read_numbers(value, "paints");
  set_aside.emblems = read_numbers(value, "emblems");
  return set_aside;
}

JsonValue set_aside_to_json(const SetAside &set_aside) {
  JsonValue::Array hangars;
  for (const auto &[index, hangar] : set_aside.hangars) {
    JsonValue::Object object = hangar_to_json(hangar);
    object.insert(object.begin(), {"index", number(index)});
    hangars.push_back(JsonValue{std::move(object)});
  }
  return JsonValue{JsonValue::Object{
      {"stock", stock_to_json(set_aside.stock)},
      {"hangars", JsonValue{std::move(hangars)}},
      {"riding", number(set_aside.riding)},
      {"paints", numbers(set_aside.paints)},
      {"emblems", numbers(set_aside.emblems)},
  }};
}

} // namespace

PartKind slot_kind(const PartSlot slot) {
  switch (slot) {
  case PartSlot::body:
    return PartKind::body;
  case PartSlot::legs:
    return PartKind::legs;
  case PartSlot::right_arm:
  case PartSlot::left_arm:
    return PartKind::arm;
  case PartSlot::right_shoulder:
  case PartSlot::left_shoulder:
    return PartKind::shoulder;
  case PartSlot::chip:
    return PartKind::chip;
  }
  return PartKind::body;
}

bool Hangar::empty() const {
  return modules.empty() && std::ranges::all_of(parts, [](const int part) { return part < 0; });
}

bool Hangar::complete() const {
  return part(PartSlot::body) >= 0 && part(PartSlot::legs) >= 0 && part(PartSlot::chip) >= 0;
}

bool SetAside::empty() const {
  return stock.empty() && hangars.empty() && riding < 0 && paints.empty() && emblems.empty();
}

int Garage::free_stock() const {
  return stock_size - static_cast<int>(stock.size() + set_aside.stock.size());
}

int Garage::add_stock(StockItem item) {
  for (int index = 0; index < stock_size; ++index) {
    if (!stock.contains(index) && !set_aside.stock.contains(index)) {
      stock.emplace(index, std::move(item));
      return index;
    }
  }
  return -1;
}

Garage new_game_garage(const int owned) {
  Garage garage;
  for (int hangar = 0; hangar < garage_hangar_count; ++hangar) {
    garage.hangars[static_cast<std::size_t>(hangar)].owned = hangar < owned;
  }
  return garage;
}

Garage garage_from_json(const JsonValue &value) {
  Garage garage;
  garage.stock_size = std::max(integer_or(value, "stock_size", garage_stock_size), 0);
  read_stock(value, garage.stock_size, garage.stock);
  if (const auto *set_aside = value.find("set_aside"); set_aside != nullptr) {
    garage.set_aside = set_aside_from_json(*set_aside, garage.stock_size);
  }
  if (const auto *hangars = value.find("hangars"); hangars != nullptr && hangars->is_array()) {
    std::size_t index = 0;
    for (const auto &entry : hangars->array()) {
      if (index == garage.hangars.size()) {
        break;
      }
      garage.hangars[index++] = hangar_from_json(entry, garage.stock);
    }
  }
  const int riding = integer_or(value, "riding", -1);
  garage.riding = riding >= 0 && riding < garage_hangar_count ? riding : -1;
  if (value.find("paints") != nullptr) {
    garage.paints = read_numbers(value, "paints");
  }
  if (value.find("emblems") != nullptr) {
    garage.emblems = read_numbers(value, "emblems");
  }
  return garage;
}

JsonValue garage_to_json(const Garage &garage) {
  JsonValue::Array hangars;
  for (const Hangar &hangar : garage.hangars) {
    hangars.push_back(JsonValue{hangar_to_json(hangar)});
  }
  JsonValue::Object object;
  object.emplace_back("stock", stock_to_json(garage.stock));
  object.emplace_back("hangars", JsonValue{std::move(hangars)});
  object.emplace_back("riding", number(garage.riding));
  object.emplace_back("paints", numbers(garage.paints));
  object.emplace_back("emblems", numbers(garage.emblems));
  if (garage.stock_size != garage_stock_size) {
    object.emplace_back("stock_size", number(garage.stock_size));
  }
  if (!garage.set_aside.empty()) {
    object.emplace_back("set_aside", set_aside_to_json(garage.set_aside));
  }
  return JsonValue{std::move(object)};
}

} // namespace phantomcrash
