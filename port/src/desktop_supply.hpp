#pragma once

#include "desktop_world_renderer.hpp"
#include "phantomcrash/effect_system.hpp"
#include "phantomcrash/battle.hpp"
#include "phantomcrash/battle_scene.hpp"
#include "phantomcrash/data_roots.hpp"
#include "phantomcrash/supply_drop.hpp"

#include <array>
#include <cstdint>
#include <memory>
#include <random>
#include <string>
#include <vector>

namespace phantomcrash {

// The supply drops (supply_drop.hpp) drawn: the carrier with its hatch and
// spinning part, the pickups falling or waiting on the ground with their
// glow, and the burst each take sends to the robot.
class DesktopSupply final {
public:
  DesktopSupply(WorldRenderer &world, const SupplyLook &look, int stage);

  [[nodiscard]] bool ready() const noexcept {
    return model_ >= 0;
  }
  // Appends the carrier's and the pickups' pieces to `items`; on a split
  // screen the pickups are tinted in place of their glow.
  void add_items(const SupplyDrop &drop, std::vector<WorldItem> &items, bool split_screen);
  // Appends the carrier's pieces alone (the finale's camera carrier).
  void add_carrier(const SupplyCarrier &carrier, std::vector<WorldItem> &items);
  // Appends the pickups' glows, facing the camera at `eye`.
  void add_sprites(const SupplyDrop &drop, Vec3 eye, std::vector<WorldSprite> &sprites) const;
  // Starts this tick's takes' bursts in `effects` and keeps the bursts in
  // flight heading for their robots (0x066e50, 0x0880ad).
  void tick(const Battle &battle, EffectSystem *effects);

private:
  struct Burst {
    EffectHandle handle{};
    int robot{};
  };
  // The glow colour and the burst level of pickup type `type`.
  [[nodiscard]] int effect_of(const SupplyDrop &drop, int type) const;

  void add_mesh(int mesh, const Mat4 &world, std::vector<WorldItem> &items) const;
  // The day or night model's mesh (0x069d52 draws each pickup from either).
  void add_pickup_mesh(std::size_t parity, int mesh, const Mat4 &world,
                       std::vector<WorldItem> &items) const;

  SupplyLook look_;
  SupplyModel parts_; // the stage parity's
  int model_{-1};
  std::vector<std::vector<int>> mesh_nodes_;
  // Both parities' models for the pickups, -1 where one did not load.
  std::array<int, 2> pickup_models_{-1, -1};
  std::array<std::vector<std::vector<int>>, 2> pickup_nodes_;
  int glow_texture_{-1};
  std::vector<Burst> bursts_;
  std::minstd_rand random_{0x5eed};
};

// Gives the battle the stage's supply drops (the routes in the scenes'
// tables) and returns their view, or null when there is none to draw.
[[nodiscard]] std::unique_ptr<DesktopSupply> load_supply(WorldRenderer *world,
                                                         const DataRoots &roots,
                                                         const SceneTuning &tuning,
                                                         SupplySettings settings, Battle &battle);
// The player's pickups and carrier shot down this tick: their money into
// `money`, a pickup's sound at the robot.
void collect_supply(const Battle &battle, const SupplyLook &look, std::int64_t &money,
                    std::vector<WorldSound> &sounds);

} // namespace phantomcrash
