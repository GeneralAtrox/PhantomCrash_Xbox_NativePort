#pragma once

#include "desktop_world_renderer.hpp"
#include "phantomcrash/battle.hpp"
#include "phantomcrash/data_roots.hpp"
#include "phantomcrash/source_muzzle_flash.hpp"

#include <memory>
#include <random>
#include <vector>

namespace phantomcrash {
class BattleView;

// Draws S.L.A.I. muzzle flashes (source_muzzle_flash.hpp) from
// slai/muzzle_flashes.json: each shot of a listed weapon shows the flash
// model's layers on its animated flash node for one battle update.
class DesktopSourceMuzzleFlashes final {
public:
  // Null when the S.L.A.I. data is not installed or cannot be read.
  static std::unique_ptr<DesktopSourceMuzzleFlashes> load(WorldRenderer &world,
                                                          const DataRoots &roots);
  explicit DesktopSourceMuzzleFlashes(SourceMuzzleFlashes data, int model)
      : data_(std::move(data)), model_(model) {}

  // Whether `weapon` shows this flash in place of the native one.
  [[nodiscard]] bool handles(const WeaponTuning &weapon) const;
  // Ages the shown flashes, then starts one for each of this tick's shots.
  void tick(const Battle &battle, const BattleView *view, float seconds);
  void add_items(std::vector<WorldItem> &items) const;

private:
  struct Shown {
    Mat4 world;
    float remaining{};
  };
  [[nodiscard]] const SourceMuzzleFlashRule *rule(const WeaponTuning &weapon) const;

  SourceMuzzleFlashes data_;
  int model_{-1};
  std::vector<Shown> shown_;
  std::minstd_rand random_{0x511780};
};
} // namespace phantomcrash
