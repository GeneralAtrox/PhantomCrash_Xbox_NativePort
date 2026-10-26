#include "desktop_source_muzzle_flashes.hpp"
#include "desktop_battle_view.hpp"
#include "phantomcrash/json_fields.hpp"
#include <SDL3/SDL_log.h>
#include <algorithm>
#include <exception>

namespace phantomcrash {
std::unique_ptr<DesktopSourceMuzzleFlashes>
DesktopSourceMuzzleFlashes::load(WorldRenderer &world, const DataRoots &roots) {
  const auto file = roots.find_asset("slai/muzzle_flashes.json");
  if (!file) {
    return nullptr;
  }
  try {
    auto data = parse_source_muzzle_flashes(json::read_file(*file));
    const int model = world.model(data.model);
    if (model < 0) {
      SDL_Log("S.L.A.I. muzzle flashes: model %s is missing", data.model.c_str());
      return nullptr;
    }
    SDL_Log("S.L.A.I. muzzle flashes: %zu weapon rules", data.rules.size());
    return std::make_unique<DesktopSourceMuzzleFlashes>(std::move(data), model);
  } catch (const std::exception &error) {
    SDL_Log("S.L.A.I. muzzle flashes: %s", error.what());
    return nullptr;
  }
}

const SourceMuzzleFlashRule *DesktopSourceMuzzleFlashes::rule(const WeaponTuning &weapon) const {
  if (!weapon.part) {
    return nullptr;
  }
  return data_.find(weapon.model, weapon.part->table == WeaponTable::shoulder);
}

bool DesktopSourceMuzzleFlashes::handles(const WeaponTuning &weapon) const {
  return rule(weapon) != nullptr;
}

void DesktopSourceMuzzleFlashes::tick(const Battle &battle, const BattleView *view,
                                      const float seconds) {
  // 0x511810: shown for the update that fired, hidden at the next.
  for (Shown &flash : shown_) {
    flash.remaining -= seconds;
  }
  std::erase_if(shown_, [](const Shown &flash) { return flash.remaining <= 0; });
  if (view == nullptr) {
    return;
  }
  std::uniform_real_distribution<float> roll(data_.roll_min, data_.roll_max);
  for (const BattleEvent &event : battle.events()) {
    if (event.kind != BattleEventKind::fired || event.weapon == nullptr) {
      continue;
    }
    const SourceMuzzleFlashRule *found = rule(*event.weapon);
    const int node = found ? source_flash_node(*found, event.barrel) : -1;
    if (node < 0) {
      continue;
    }
    if (const auto frame = view->source_weapon_frame(battle, event.robot, event.slot, node)) {
      shown_.push_back({source_flash_transform(*found, roll(random_)) * *frame, data_.seconds});
    }
  }
}

void DesktopSourceMuzzleFlashes::add_items(std::vector<WorldItem> &items) const {
  for (const Shown &flash : shown_) {
    // 0x50d600: two instances of the model, at alpha 0.4 and 0.8.
    for (const float alpha : data_.alphas) {
      WorldItem item;
      item.model = model_;
      item.world = flash.world;
      item.reflected = reflects(flash.world);
      item.alpha = alpha;
      item.additive = true;
      item.local_light = false;
      items.push_back(std::move(item));
    }
  }
}
} // namespace phantomcrash
