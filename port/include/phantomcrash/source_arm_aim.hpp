#pragma once

#include "phantomcrash/json.hpp"
#include "phantomcrash/math3d.hpp"

#include <array>
#include <cstddef>
#include <optional>
#include <vector>

namespace phantomcrash {
struct RigidNode;

// One weapon slot's row of the original arm aim table (0x59a910, read by
// src/phantomcrash_research/slai/arm_aim.py). Angles are radians about the
// source's axes; the model export mirrors y, which aim_source_arm undoes.
struct SourceArmAimSide {
  int node{-1}; // the body's upper-arm node 0x4e7a90 turns
  float pitch{};
  float yaw{};
  float roll{};
  std::array<float, 2> pitch_limits{};
  float twist{}; // 0x4e7f60's turn about the barrel, negated for slot 0
};

// The row 0x4e7a90 picks by the legs (0x254890: the legs record's +6).
// Port side s is source weapon slot s.
struct SourceArmAim {
  std::array<SourceArmAimSide, 2> sides;
  std::vector<char> order; // 'x', 'y', 'z': the turns, first applied first
  // False sets the node's world rotation to the turn, then the body's yaw
  // (the row's +0x2b flag; 0x4e7db4..4e7f14).
  bool keep_rotation{true};
};

// The arm raise counts ticks of the source clock, taken as 60 Hz (an
// adapter, as the S.L.A.I. skeletons' native_clock_rate).
inline constexpr float source_ticks_per_second = 60.0F;

// What one arm aims at this frame: the raise weight (RobotCombat, 0..1),
// the aim point and the robot's position, all in world space.
struct ArmAimState {
  float weight{};
  Vec3 target{};
  Vec3 origin{};
};

[[nodiscard]] std::optional<SourceArmAim> parse_source_arm_aim(const json::JsonValue *value);

// 0x4e7a90: raises the side's upper-arm node towards the target. `body` is
// the body rig's root frame; the turn is made in the node's own space.
void aim_source_arm(const SourceArmAim &aim, std::size_t side, const ArmAimState &state,
                    const Mat4 &body, std::vector<RigidNode> &nodes);

// 0x4e7f60: turns a weapon's aim node so its -z faces the target, rolled by
// `twist`, weighted by the raise. A mirrored side keeps its mirror.
void aim_source_weapon(int node, float twist, const ArmAimState &state, const Mat4 &root,
                       std::vector<RigidNode> &nodes);

// The rotation part of `rotation` (row vectors, orthonormal) turned only
// `weight` of the way from identity, along the shortest arc.
[[nodiscard]] Mat4 partial_rotation(const Mat4 &rotation, float weight);
} // namespace phantomcrash
