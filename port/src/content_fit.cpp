#include "phantomcrash/content_fit.hpp"

#include <algorithm>
#include <ranges>
#include <set>

namespace phantomcrash {
namespace {

int list_size(const json::JsonValue &document, const char *key) {
  const auto *list = document.find(key);
  return list != nullptr && list->is_array() ? static_cast<int>(list->array().size()) : 0;
}

// The stock entries a hangar has fitted.
std::set<int> fitted_stock(const Hangar &hangar) {
  std::set<int> fitted(hangar.modules.begin(), hangar.modules.end());
  for (const int part : hangar.parts) {
    if (part >= 0) {
      fitted.insert(part);
    }
  }
  return fitted;
}

bool hangar_fits(const Hangar &hangar, const std::map<int, StockItem> &stock,
                 const ContentLimits &limits) {
  if (hangar.paint >= limits.paints || hangar.emblem >= limits.emblems) {
    return false;
  }
  return std::ranges::all_of(fitted_stock(hangar), [&](const int index) {
    const auto entry = stock.find(index);
    return entry == stock.end() || limits.holds(entry->second);
  });
}

void move_entry(std::map<int, StockItem> &from, std::map<int, StockItem> &to, const int index) {
  if (const auto entry = from.find(index); entry != from.end() && !to.contains(index)) {
    to.emplace(index, std::move(entry->second));
    from.erase(entry);
  }
}

// Moves the ids under `limit` from `from` to `to` (fits: true) or the rest.
void move_ids(std::vector<int> &from, std::vector<int> &to, const int limit, const bool fits) {
  const auto moves = [&](const int id) { return (id < limit) == fits; };
  for (const int id : from) {
    if (moves(id) && std::ranges::find(to, id) == to.end()) {
      to.push_back(id);
    }
  }
  std::erase_if(from, moves);
}

void give_back(Garage &garage, const ContentLimits &limits) {
  SetAside &aside = garage.set_aside;
  for (auto entry = aside.hangars.begin(); entry != aside.hangars.end();) {
    const auto [index, hangar] = *entry;
    Hangar &slot = garage.hangars[static_cast<std::size_t>(index)];
    if (slot.owned || !slot.empty() || !hangar_fits(hangar, aside.stock, limits)) {
      ++entry;
      continue;
    }
    for (const int stock : fitted_stock(hangar)) {
      move_entry(aside.stock, garage.stock, stock);
    }
    slot = hangar;
    entry = aside.hangars.erase(entry);
  }
  std::set<int> held;
  for (const auto &[index, hangar] : aside.hangars) {
    held.merge(fitted_stock(hangar));
  }
  std::vector<int> loose;
  for (const auto &[index, item] : aside.stock) {
    if (!held.contains(index) && limits.holds(item)) {
      loose.push_back(index);
    }
  }
  for (const int index : loose) {
    move_entry(aside.stock, garage.stock, index);
  }
  if (aside.riding >= 0 && !garage.locked(aside.riding)) {
    if (garage.riding < 0) {
      garage.riding = aside.riding;
    }
    aside.riding = -1;
  }
  move_ids(aside.paints, garage.paints, limits.paints, true);
  move_ids(aside.emblems, garage.emblems, limits.emblems, true);
}

void set_aside(Garage &garage, const ContentLimits &limits) {
  SetAside &aside = garage.set_aside;
  for (int index = 0; index < garage_hangar_count; ++index) {
    Hangar &slot = garage.hangars[static_cast<std::size_t>(index)];
    if (!slot.owned || hangar_fits(slot, garage.stock, limits) || garage.locked(index)) {
      continue;
    }
    for (const int stock : fitted_stock(slot)) {
      move_entry(garage.stock, aside.stock, stock);
    }
    aside.hangars.emplace(index, std::move(slot));
    slot = Hangar{};
    if (garage.riding == index) {
      aside.riding = index;
      garage.riding = -1;
    }
  }
  std::vector<int> unknown;
  for (const auto &[index, item] : garage.stock) {
    if (!limits.holds(item)) {
      unknown.push_back(index);
    }
  }
  for (const int index : unknown) {
    move_entry(garage.stock, aside.stock, index);
  }
  move_ids(garage.paints, aside.paints, limits.paints, false);
  move_ids(garage.emblems, aside.emblems, limits.emblems, false);
}

} // namespace

bool ContentLimits::valid() const {
  return bodies > 0 && legs > 0 && arm_weapons > 0 && shoulder_weapons > 0 && chips > 0 &&
         modules > 0 && paints > 0 && emblems > 0;
}

bool ContentLimits::holds(const StockItem &item) const {
  switch (item.kind) {
  case PartKind::body:
    return item.id < bodies;
  case PartKind::legs:
    return item.id < legs;
  case PartKind::arm:
    return item.id < arm_weapons;
  case PartKind::shoulder:
    return item.id < shoulder_weapons;
  case PartKind::chip:
    return item.id < chips;
  case PartKind::module:
    return item.id < modules;
  }
  return false;
}

ContentLimits content_limits(const json::JsonValue &parts, const json::JsonValue &garage) {
  return {list_size(parts, "bodies"),      list_size(parts, "legs"),
          list_size(parts, "arm_weapons"), list_size(parts, "shoulder_weapons"),
          list_size(parts, "chips"),       list_size(garage, "modules"),
          list_size(garage, "paints"),     list_size(garage, "emblems")};
}

void fit_garage(Garage &garage, const ContentLimits &limits) {
  if (!limits.valid()) {
    return;
  }
  give_back(garage, limits);
  set_aside(garage, limits);
}

void fit_settings(Settings &settings, const ContentLimits &limits) {
  const auto fit = [&limits](std::optional<QuestSave> &quest) {
    if (quest) {
      fit_garage(quest->garage, limits);
    }
  };
  fit_garage(settings.quest.garage, limits);
  std::ranges::for_each(settings.save_slots, fit);
  std::ranges::for_each(settings.coop_profiles, fit);
  fit(settings.online_profile);
}

} // namespace phantomcrash
