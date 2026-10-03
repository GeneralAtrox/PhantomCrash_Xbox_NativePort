#pragma once

#include "phantomcrash/json.hpp"

#include <array>
#include <string>

namespace phantomcrash {

// How the robots' shadows look (data/world/training.json "robot_shadows").
//
// 0x082e60 gives a player's robot a stencil volume (view layer 3), unless
// the screen is split (+0x11074). Every other robot has, while it stands
// on the ground (+0x252c), a flat shadow shaped for its legs: mesh
// 0x242750[leg type] of resource 184, placed with the robot (+0x370) and
// coloured 0x60ffffff (view layer 8). The robots themselves are drawn
// after the volumes and clear the stencil (layers 4 to 6), so a volume
// darkens the stage but never a robot.
struct RobotShadowLook {
  bool volumes{true};               // players' robots cast volumes
  bool split_screen_volumes{false}; // ... in the split screen too
  float volume_length{50.0F};       // how far a volume reaches from the robot (0x0c2637 c4.x)
  std::string ground_model;         // the flat shadows (resource 184)
  std::array<float, 4> ground_colour{1.0F, 1.0F, 1.0F, 0x60 / 255.0F};
};

enum class RobotShadow { none, volume, ground };

// The shadow a robot has this frame.
[[nodiscard]] RobotShadow robot_shadow(const RobotShadowLook &look, bool piloted, bool split_screen,
                                       bool grounded);

[[nodiscard]] RobotShadowLook parse_robot_shadow_look(const json::JsonValue &value);

} // namespace phantomcrash
