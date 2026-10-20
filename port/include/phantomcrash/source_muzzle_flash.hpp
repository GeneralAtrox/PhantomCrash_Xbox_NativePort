#pragma once

#include "phantomcrash/json.hpp"
#include "phantomcrash/math3d.hpp"
#include <array>
#include <string>
#include <vector>

namespace phantomcrash {
// S.L.A.I.'s muzzle flash. The builder (0x50d600) gives each mount two
// instances of one flash model, attached to the weapon's first flash node
// from muzlle_pos.dat and scaled per weapon. A shot (0x511630) shows them for
// one battle update (0x511810 counts +0x88 down and hides them); six arm
// weapons attach them to the second node once the shot leaves a later barrel.
// Every update rolls the flash about its z axis (0x511780).
struct SourceMuzzleFlashRule {
  int model{-1}; // WeaponTuning::model
  bool shoulder{};
  std::vector<int> nodes;
  bool barrel_flash{};
  float scale{1.0F};
};

struct SourceMuzzleFlashes {
  std::string model;
  std::vector<float> alphas;
  std::array<float, 3> colour{1.0F, 1.0F, 1.0F};
  float seconds{1.0F / 30.0F}; // visible updates over the battle update rate
  float roll_min{-pi};
  float roll_max{pi};
  std::vector<SourceMuzzleFlashRule> rules;
  [[nodiscard]] const SourceMuzzleFlashRule *find(int model, bool shoulder) const;
};

[[nodiscard]] SourceMuzzleFlashes parse_source_muzzle_flashes(const json::JsonValue &value);
// The weapon node the flash attaches to for a shot from `barrel`; -1 if none.
[[nodiscard]] int source_flash_node(const SourceMuzzleFlashRule &rule, int barrel);
// The flash model's placement under that node: its roll, then its scale.
[[nodiscard]] Mat4 source_flash_transform(const SourceMuzzleFlashRule &rule, float roll);
} // namespace phantomcrash
