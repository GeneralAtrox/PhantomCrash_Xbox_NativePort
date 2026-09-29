#pragma once

#include "desktop_world_renderer.hpp"
#include "phantomcrash/battle.hpp"
#include "phantomcrash/effect_system.hpp"

#include <optional>
#include <random>
#include <vector>

namespace phantomcrash {

// The battle's shots and what they set off, from the effect bank's tables
// (effect_bank.hpp): each shot drawn with its look's model (0x09831e), a
// trailing missile's exhaust behind it; the impact group a landing shot
// starts (0x098eda, 0x065f28) and the groups a wreck starts (0x07675e);
// and the group flashes, drawn as sprites facing the camera (0x065ade).
class DesktopShots final {
public:
  // `bank` must outlive this (DesktopEffects' system keeps it).
  DesktopShots(WorldRenderer &world, const EffectBank &bank);

  // Starts the effects of this tick's landings, wrecks and blasts.
  void tick(const Battle &battle, EffectSystem &effects);
  // Appends the shots' models; billboard looks face the camera of `view`.
  void add_items(const Battle &battle, const Mat4 &view, std::vector<WorldItem> &items);
  // Appends the sprite shots and the flashes in play.
  void add_sprites(const Battle &battle, const EffectSystem &effects,
                   std::vector<WorldSprite> &sprites) const;

private:
  struct Mesh {
    int model{-1};
    std::vector<int> nodes;
  };
  [[nodiscard]] Mesh load_mesh(WorldRenderer &world, const std::optional<EffectPicture> &picture);
  static void add_mesh(const Mesh &mesh, const Mat4 &world, std::vector<WorldItem> &items);

  const EffectBank *bank_;
  std::vector<Mesh> looks_;
  std::vector<int> look_sprites_;
  Mesh exhaust_;
  std::vector<std::vector<int>> flash_textures_;
  std::minstd_rand random_{0x5eed};
};

} // namespace phantomcrash
