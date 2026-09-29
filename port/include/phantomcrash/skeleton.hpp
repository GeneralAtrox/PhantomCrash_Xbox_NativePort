#pragma once

#include "phantomcrash/json.hpp"
#include "phantomcrash/math3d.hpp"
#include "phantomcrash/motion.hpp"

#include <array>
#include <cstdint>
#include <optional>
#include <vector>

namespace phantomcrash {

// Robot skeletons (robots/skeletons.json, from the static trees in the XBE;
// see src/phantomcrash_research/robots.py). A skeleton does not belong to a
// model: its nodes fill bone slots, and each drawn node of the robot's part
// models is skinned rigidly to one slot (docs/ROBOTS.md).

enum class SkeletonNodeType : int {
  plain = 0,
  arm_joint = 1,
  ik_start = 2, // a leg chain: its first child is aimed (one bone) or bent
  ik_bone = 3,  // (two bones) to reach the track of the chain's end
  ik_end = 4,
  arms = 7, // the two arm skeletons hang here
};

inline constexpr std::uint32_t node_root_motion = 0x2;
inline constexpr std::uint32_t node_turns_with_aim = 0x4;
inline constexpr std::uint32_t node_pitches_with_aim = 0x100;
inline constexpr std::uint32_t node_effect_point = 0x20000;
inline constexpr std::uint32_t node_forearm = 0x30; // 0x10 one side, 0x20 the other

struct SkeletonNode {
  SkeletonNodeType type{SkeletonNodeType::plain};
  std::uint32_t flags{};
  int track{-1};
  Vec3 translation{}; // as stored; the evaluator mirrors it into game space
  Vec3 rotation{};
  int bone{-1};
  std::vector<SkeletonNode> children;
};

struct RobotSkeletons {
  std::vector<std::optional<SkeletonNode>> legs;                // by leg type
  std::array<std::vector<std::optional<SkeletonNode>>, 2> arms; // [side][arm type]
};

[[nodiscard]] SkeletonNode parse_skeleton_node(const json::JsonValue &value);
[[nodiscard]] RobotSkeletons parse_robot_skeletons(const json::JsonValue &document);

// A motion playing at a frame, optionally fading in from the one before it.
struct MotionSource {
  const Motion *motion{};
  float frame{};
  const Motion *previous{};
  float previous_frame{};
  float weight{1.0F}; // of `motion` against `previous`

  [[nodiscard]] TrackSample sample(int track) const;
};

inline constexpr int bone_slots = 20;
inline constexpr int pose_parts = 3; // 0: body and legs, 1: left arm, 2: right arm

struct PosePart {
  std::array<Mat4, bone_slots> bones{};
  std::array<bool, bone_slots> set{};
};

using Pose = std::array<PosePart, pose_parts>;

struct PoseInput {
  Mat4 root;         // the robot's ground point and heading (yaw, then position)
  MotionSource legs; // drives the body-and-legs skeleton
  std::array<MotionSource, 2> arms;
  std::array<const SkeletonNode *, 2> arm_skeletons{};
  float aim_yaw{};      // turns the nodes flagged 0x4 (the upper body)
  float aim_pitch{};    // pitches the nodes flagged 0x100, within +-0.4 rad
  float forearm_turn{}; // added about x to the forearms (node_forearm) otherwise
  bool keep_sway{};     // idle and walking keep the body's sideways sway
};

// Poses the body-and-legs skeleton `root` (and the arms under it) into
// world-space bone matrices, reproducing the pose builder at 0x0aa499.
void build_pose(const SkeletonNode &root, const PoseInput &input, Pose &pose);

// D3DX rotation matrices (row vectors, left-handed).
[[nodiscard]] Mat4 rotation_x(float angle);
[[nodiscard]] Mat4 rotation_y(float angle);
[[nodiscard]] Mat4 rotation_z(float angle);

} // namespace phantomcrash
