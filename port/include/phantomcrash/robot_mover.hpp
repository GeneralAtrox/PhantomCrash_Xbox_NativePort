#pragma once

#include "phantomcrash/json.hpp"
#include "phantomcrash/math3d.hpp"

#include <array>

namespace phantomcrash {

// Leg motions by their index in a leg motion bank, as the movement code
// starts them (0x08ddb2 and its callers). The arm banks play the same index.
namespace robot_motion {
inline constexpr int walk = 0;
inline constexpr int walk_back = 1;
inline constexpr int dash = 2;      // 3 and 4 while turning
inline constexpr int dash_back = 5; // 6 and 7 while turning
inline constexpr int turn_right = 8;
inline constexpr int turn_left = 9;
inline constexpr int jump = 10;
inline constexpr int air = 11;
inline constexpr int dodge_left = 13; // 0x08c0c1 counts 13 and 15 as one side
inline constexpr int dodge_right = 14;
inline constexpr int air_dodge_left = 15;
inline constexpr int air_dodge_right = 16;
// The hit reactions (0x08dcd1), by the way the hit pushes the robot: back,
// forward, to its left or its right; in the air, those 12 later.
inline constexpr int hit_back = 17;
inline constexpr int hit_forward = 18;
inline constexpr int hit_left = 19;
inline constexpr int hit_right = 20;
inline constexpr int air_hit_offset = 12;
inline constexpr int brake = 27;
inline constexpr int stand = 28;
} // namespace robot_motion

// How a robot moves. The original works these out from the leg part's
// stats and tuning levels; until that is recovered they are port values
// (data/world/training.json "movement"), except where noted.
struct MovementTuning {
  float walk_speed{7.5F}; // units per second at full stick
  float back_speed{5.5F};
  float walk_accel{30.0F}; // units per second squared
  float dash_speed{24.0F};
  float dash_back_speed{18.0F};
  // How fast the walk and dash motions play (robot +0x68, +0x70): the
  // robot's load sets them (robot_load.hpp) and the speeds above with them.
  float walk_motion_rate{1.0F};
  float dash_motion_rate{1.0F};
  float turn_degrees_per_second{110.0F};
  float dash_turn_degrees_per_second{70.0F};
  float stick_threshold{0.01F};   // 0x199f78: the stick counts as pushed past this
  float dash_stick{0.7F};         // 0x17cd14: holding past this ...
  float dash_hold_seconds{0.5F};  // ... for 30 frames (0x17ccfc) starts a dash
  float jump_speed{12.0F};        // upward speed at take-off
  float jump_boost{40.0F};        // extra lift while A stays held ...
  float jump_boost_seconds{0.5F}; // ... for at most this long
  // 0x07d117: 9.8 / (rate / 2)^2 a tick at the battle's 30 Hz, times the
  // world's 2.5 (0x046aa5); 9.8 * 2.5 units/s^2.
  float gravity{24.5F};
  float air_control{0.5F}; // share of the walk speed steerable in the air
  float dodge_speed{32.0F};
  float dodge_seconds{0.3F};
  // A flinch lasts at least as long as the hit reactions' motions, 29
  // frames (motions 17-20 and 29-32).
  float hit_seconds{29.0F / 60.0F};
  // The twist (0x07d330): 3 degrees a battle tick (0x19f8f8) at 30 Hz,
  // within 45 degrees (0x17d0e0) plus 22.5 (0x1b5af0) times the stick's
  // smoothed push, which keeps 0.9 of itself a tick (0x1b5af4). The reset
  // eases it back to straight over 8 ticks (0x17beec).
  float twist_degrees_per_second{90.0F};
  float twist_limit_degrees{45.0F};
  float twist_stretch_degrees{22.5F};
  float twist_stick_keep{0.9F}; // per battle tick
  float twist_reset_seconds{8.0F / 30.0F};
  float pitch_limit{0.4F}; // 0x0aa499 clamps the aim pitch to +-0.4 rad
  float pitch_per_second{1.2F};
};

[[nodiscard]] MovementTuning parse_movement_tuning(const json::JsonValue &value);

// One tick of a robot's controls, from the pad or from the AI.
struct RobotControl {
  std::array<float, 2> move{};      // left stick, +y forward, +x right
  float twist{};                    // right stick x: turn the upper body
  float pitch{};                    // right stick y: aim up and down
  std::array<float, 2> mouse_aim{}; // direct yaw/pitch displacement, radians
  bool jump{};                      // A held
  bool dash{};                      // left stick clicked this tick
  int dodge{};                      // -1 left, +1 right (white / black pressed)
  bool reset_twist{};               // right stick clicked this tick
};

enum class MoveMode { stand, walk, turn, dash, jump, dodge, brake, hit, scripted };

// One tick of a scripted walk (the battle's entrance and exit, 0x08928c and
// 0x08a161): the robot moves at `velocity` along the ground facing
// `heading` and plays `motion` at `motion_speed`; its controls do nothing.
struct ScriptedStep {
  Vec3 velocity; // world units a second
  float heading{};
  int motion{robot_motion::stand};
  bool loops{true};
  float motion_speed{1.0F};
};

// A robot's movement over the ground: walking, turning on the spot,
// dashing, jumping, dodging and the upper-body twist, with the leg motion
// each state plays. Collision is left to the caller (Battle).
class RobotMover final {
public:
  explicit RobotMover(const MovementTuning &tuning) : tuning_(tuning) {}
  void set_tuning(const MovementTuning &tuning) noexcept {
    tuning_ = tuning;
  }
  [[nodiscard]] const MovementTuning &tuning() const noexcept {
    return tuning_;
  }

  void place(Vec3 position, float heading);
  void tick(const RobotControl &control, float seconds);
  // A hit that pushes the robot along `push` (world space): it plays the
  // hit reaction for that direction (0x08dcd1) for `seconds`, at least
  // hit_seconds, stopped on the ground and falling uncontrolled in the air.
  void stagger(float seconds, Vec3 push);
  // Drives the robot for the next tick (see ScriptedStep).
  void script(const ScriptedStep &step);
  // Hands the robot back to its controls, standing.
  void release();

  [[nodiscard]] Vec3 position() const noexcept {
    return position_;
  }
  void set_position(Vec3 position) noexcept {
    position_ = position;
  }
  // The ground height under the robot, for the next tick (no_ground: none).
  void set_floor(float height) noexcept {
    floor_ = height;
  }
  // Stands the robot on ground at `height` (a surface it was pushed onto).
  void land(float height);
  // Stops the rise of a jump (a ceiling).
  void bump_head();
  [[nodiscard]] float heading() const noexcept {
    return heading_;
  }
  [[nodiscard]] float twist() const noexcept {
    return twist_;
  }
  [[nodiscard]] float pitch() const noexcept {
    return pitch_;
  }
  // Where the upper body faces: heading plus twist.
  [[nodiscard]] float aim_heading() const noexcept {
    return heading_ + twist_;
  }
  [[nodiscard]] MoveMode mode() const noexcept {
    return mode_;
  }
  [[nodiscard]] bool backward() const noexcept {
    return backward_;
  }
  [[nodiscard]] bool grounded() const noexcept {
    return grounded_;
  }
  // Velocity in the robot's own frame: +z forward, +x right.
  [[nodiscard]] Vec3 local_velocity() const noexcept {
    return local_velocity_;
  }
  [[nodiscard]] Vec3 velocity() const;
  // The leg motion for the current state and whether it loops.
  [[nodiscard]] int motion() const noexcept {
    return motion_;
  }
  [[nodiscard]] bool motion_loops() const noexcept {
    return loops_;
  }
  // Motion playback speed (the walk slows with a half-pressed stick).
  [[nodiscard]] float motion_speed() const noexcept {
    return motion_speed_;
  }
  // The dodge taken this tick (-1, +1) or 0.
  [[nodiscard]] int dodged() const noexcept {
    return dodged_;
  }
  [[nodiscard]] float mode_seconds() const noexcept {
    return mode_seconds_;
  }

private:
  void set_mode(MoveMode mode, int motion, bool loops);
  void tick_ground(const RobotControl &control, float seconds);
  void tick_air(const RobotControl &control, float seconds);
  void tick_twist(const RobotControl &control, float seconds);
  void turn(float stick, float degrees_per_second, float seconds);

  MovementTuning tuning_;
  Vec3 position_{};
  float heading_{};
  float twist_{};
  float twist_stick_{}; // the stick's smoothed push, 0..1
  float twist_reset_from_{};
  float twist_reset_left_{};
  float pitch_{};
  MoveMode mode_{MoveMode::stand};
  int motion_{robot_motion::stand};
  bool loops_{true};
  float motion_speed_{1.0F};
  float mode_seconds_{};
  bool backward_{};
  bool grounded_{true};
  float floor_{};
  Vec3 local_velocity_{};
  float vertical_speed_{};
  float dash_hold_{};
  float boost_left_{};
  float stagger_left_{};
  int dodged_{};
};

// The original runs battles at 30 ticks a second (0x255ab0, scene 42).
inline constexpr float battle_tick_rate = 30.0F;

// Ground this far below the feet still holds a walking robot (port value).
inline constexpr float ground_snap = 0.5F;

// The unit vector a heading faces along the ground: robots face -z of
// their own frame (the walk moves them by (0, 0, -speed), 0x08e6b6) and
// RotY(heading) turns that frame.
[[nodiscard]] Vec3 heading_forward(float heading);
// The heading that faces along `direction` (the inverse of heading_forward).
[[nodiscard]] float heading_toward(Vec3 direction);
// The robot's right-hand side for `heading`.
[[nodiscard]] Vec3 heading_right(float heading);
// `angle` wrapped into -pi..pi.
[[nodiscard]] float wrap_angle(float angle);

} // namespace phantomcrash
