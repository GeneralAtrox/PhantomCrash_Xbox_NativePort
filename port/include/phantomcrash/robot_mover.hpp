#pragma once

#include "phantomcrash/json.hpp"
#include "phantomcrash/math3d.hpp"
#include "phantomcrash/robot_motion.hpp"

#include <array>
#include <cstdint>
#include <optional>
#include <vector>

namespace phantomcrash {

struct MotionTiming; // robot.hpp

// How a robot moves (data/world/training.json "movement"). A robot with
// legs records takes its speeds from them (robot_load.hpp); the walk and
// dash speeds here are then its motions' strides, or are replaced, and
// stay port values only for a robot without legs records.
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
  // Whether the walk and dash speeds above are their motions' strides
  // (stride_tuning) rather than the defaults.
  bool walk_stride{};
  bool dash_stride{};
  // The turns on the ground, in a dash and in the air. The original turns
  // at one rate in all three (0x08e257 reads robot +0x7c, 0x0905af +0x78,
  // both the legs' turn under the load); robot_load sets them from the
  // legs. These defaults are for a robot without legs records.
  float turn_degrees_per_second{110.0F};
  float dash_turn_degrees_per_second{70.0F};
  float air_turn_degrees_per_second{55.0F};
  float stick_threshold{0.01F};   // 0x199f78: the stick counts as pushed past this
  float dash_stick{0.7F};         // 0x17cd14: holding past this ...
  float dash_hold_seconds{0.5F};  // ... for 30 frames (0x17ccfc) starts a dash
  float brake_cut_seconds{0.35F}; // the stick may cut a dash's stop short after this
  // 0x08f5d8: the leg types (0x05f788) whose dashes lean into a turn (their
  // movement-sound family, 0x0823cb, is 3 to 6, or they are type 4, not 20),
  // past this eased steer (0x19b550), eased a sixteenth a tick (0x17d0e4).
  std::vector<int> dash_turn_legs{2, 3, 4, 8, 9, 13, 14, 15};
  float dash_turn_stick{0.3F};
  float dash_turn_ease_ticks{16.0F};
  // The Standard controls (0x0774ad, robot +0x2740 bit 0). The left stick
  // pushed past `flick_stick` to one side, let go and pushed there again,
  // each within `flick_seconds`, dodges to that side. In the air the left
  // stick's x turns instead of strafing, unless the right stick's x is
  // pushed past `standard_air_turn_stick`.
  float flick_stick{0.5F};               // 0x17accc, 0x17be84
  float flick_seconds{5.0F / 30.0F};     // 5 ticks (+0x253c)
  float standard_air_turn_stick{0.001F}; // 0x19f8d0
  // Preferred with BEGINNER MODE (0x07d330, robot +0x273c): the right stick
  // past quick_turn_stick queues a turn of quick_turn_degrees, taken at up
  // to quick_turn_share of the twist rate; the left stick cancels it and
  // the opposite push stops it for quick_turn_cooldown_seconds.
  float quick_turn_stick{0.5F};                    // 0x17accc, 0x17be84
  float quick_turn_degrees{90.0F};                 // 0x3fc90fdb, pi/2
  float quick_turn_share{0.8F};                    // 0x19b528
  float quick_turn_cooldown_seconds{8.0F / 30.0F}; // 8 ticks (+0x2744)
  // The jump (robot +0x74): the height a held jump reaches. The take-off
  // speed and the lift that holds it up follow from it (jump_takeoff).
  float jump_height{16.0F};
  // 0x0905af / 0x04476d: the world's 6.0 times 9.8, in addition to gravity.
  float down_boost_acceleration{58.8F};
  // 0x07d117: 9.8 / (rate / 2)^2 a tick at the battle's 30 Hz, times the
  // world's 2.5 (0x046aa5); 9.8 * 2.5 units/s^2.
  float gravity{24.5F};
  float air_control{0.5F};      // share of the walk speed steerable in the air
  float dodge_speed{32.0F};     // units a second; the legs' push (robot_load.hpp)
  float air_dodge_speed{32.0F}; // the same in the air
  // The dodge runs on its motion's frames (0x08ef79): the robot goes on as
  // it moved until the loop start, slides on the push, which keeps
  // `dodge_keep` of itself a tick, until the loop end, and slows to the
  // last frame: on the ground to the push times `dodge_ease` for each frame
  // left plus one, in the air keeping `air_dodge_keep` of its speed a tick.
  float dodge_keep{0.99F};    // 0x1b5ac0
  float dodge_ease{0.05F};    // 0x17bec4
  float air_dodge_keep{0.9F}; // 0x1b5af4
  float dodge_seconds{0.3F};  // the slide when a motion has no frames (port value)
  // A melee strike (mover state 9, 0x08fc1e) plays its strike motion
  // (StrikeStart). While its blade is armed (phases 1 and 2, robot
  // +0x25a5) the robot steps forward 0.25 units a frame (0x199fd4). One
  // whose lock has completed lunges at the locked robot instead (0x091245):
  // it turns toward it by up to lunge_turn_degrees a tick (0x1b5b4c; a
  // computer robot 0x1b5b78) and moves the whole way there in a tick, but
  // no faster than its dash motion's rate times lunge_rate_speed (robot
  // +0x25bc, 0x075a36: twice the rate, 1.4 times for a computer robot
  // (0x1b59d4), in units a frame), and not at all while it faces more than
  // lunge_cone_degrees away (0x19f870). It climbs toward the target's
  // height at a tenth of the gap a frame (0x17cd04), within the jump's
  // take-off speed, and gravity does not pull it.
  float strike_speed{15.0F};
  float lunge_rate_speed{120.0F};
  float computer_lunge_rate_speed{84.0F};
  float lunge_turn_degrees{11.25F}; // a battle tick
  float computer_lunge_turn_degrees{7.875F};
  float lunge_cone_degrees{90.0F};
  float lunge_climb_rate{6.0F}; // of the height between them, a second
  // A flinch lasts at least as long as the hit reactions' motions, 29
  // frames (motions 17-20 and 29-32).
  float hit_seconds{29.0F / 60.0F};
  // The twist (robot +0xf8, 0x07d330 with the Standard controls): the
  // legs' turn (robot +0x7c) times the 60 Hz frames a tick and the stick,
  // so loaded_movement sets the rate; within 45 degrees (0x17d0e0). The
  // stretch, the stick's smoothed push (keeping twist_stick_keep a tick,
  // 0x1b5af4) times twist_stretch_degrees past the limit, is the camera's
  // look (0x1b5af0) and stays 0 here. The reset eases it back to straight
  // over 8 ticks (0x17beec).
  float twist_degrees_per_second{90.0F};
  float twist_limit_degrees{45.0F};
  float twist_stretch_degrees{0.0F};
  float twist_stick_keep{0.9F}; // per battle tick
  float twist_reset_seconds{8.0F / 30.0F};
  // The aim pitch follows the camera's look (+0x118 +0x40): 3 degrees a
  // battle tick times the stick (0x19f8f8); 0x0aa499 clamps it to +-0.4 rad.
  float pitch_limit{0.4F};
  float pitch_per_second{1.5707964F};
};

[[nodiscard]] MovementTuning parse_movement_tuning(const json::JsonValue &value);

// How a jump of a given height goes (0x075a36, 0x08ea5b, 0x0905af): the
// robot leaves the ground at `speed` (robot +0x2560), a share of the speed
// that would throw it the whole height, plus a tick of gravity. While the
// jump stays held its vertical speed eases toward `speed` and the lift
// cancels gravity for `lift_seconds` (the budget at +0x2564, spent a tick
// of gravity a tick), long enough to rise a share of the height; the throw
// after it covers the rest. A tap rises only that throw.
struct JumpTakeoff {
  float speed{};        // units a second
  float lift_seconds{}; // the most the lift can hold
};
inline constexpr float jump_takeoff_share = 0.4F; // 0x17e354
inline constexpr float jump_lift_share = 0.84F;   // 0x1b5ab0
inline constexpr float jump_lift_ease = 0.1F;     // 0x1b5b70, of the take-off speed a tick
[[nodiscard]] JumpTakeoff jump_takeoff(float height, float gravity);

// A melee strike as the fire code starts it (0x09c337): the arm's strike
// motion (strike_motion) and its frames from the leg type's timing table,
// and where the locked robot is when the lock has completed (the lunge).
struct StrikeStart {
  int motion{};
  float armed_frames{}; // the blade disarms here (the motion's loop_end, +0x211c)
  float last_frames{};  // and the strike ends here (its last, +0x2120)
  std::optional<Vec3> lunge_at;
  bool piloted{}; // robot +8 not -1: the player's lunge turn and speed
};

// The strike motion of an arm weapon (0x09dd24): by the weapon record's
// model (+8), 0x19 for the left arm and 0x1a for the right with model 7,
// 0x17 and 0x18 with any other; 10 later in the air (0x09c337).
inline constexpr int strike_motion_model = 7;
[[nodiscard]] int strike_motion(int weapon_model, int side, bool grounded);

// One tick of a robot's controls, from the pad or from the AI.
struct RobotControl {
  std::array<float, 2> move{};      // steering yaw and forward travel (AI and legacy controls)
  float strafe{};                   // independent lateral travel, positive right
  float twist{};                    // right stick x: turn the upper body
  float pitch{};                    // right stick y: aim up and down
  std::array<float, 2> mouse_aim{}; // direct yaw/pitch displacement, radians
  bool jump{};                      // A held
  bool dash{};                      // left stick clicked this tick
  int dodge{};                      // -1 left, +1 right (white / black pressed)
  bool reset_twist{};               // right stick clicked this tick
  bool down_boost{};                // an airborne fast-descent request
  bool standard{};                  // the Standard control type's stick rules
  bool quick_turn{};                // twist is Preferred's BEGINNER MODE quick turn
};

enum class MoveMode {
  stand,
  walk,
  turn,
  dash,
  jump,
  dodge,
  brake,
  hit,
  strike,
  scripted,
  destroyed
};

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

  // The leg type's motion frames (robot.hpp), which time the dodge, and
  // its index; Battle points them at the robot's assembly before each tick.
  void set_timings(const std::vector<MotionTiming> *timings, const int leg_type = -1) noexcept {
    timings_ = timings;
    leg_type_ = leg_type;
  }
  void place(Vec3 position, float heading);
  void tick(const RobotControl &control, float seconds);
  // A hit that pushes the robot along `push` (world space): it plays the
  // hit reaction for that direction (0x08dcd1) for `seconds`, at least
  // hit_seconds, stopped on the ground and falling uncontrolled in the air.
  void stagger(float seconds, Vec3 push);
  // Starts a melee strike (0x09c337), unless one is under way.
  bool strike(const StrikeStart &start);
  // Where the lunging strike's target is this tick; none once it is gone.
  void follow_lunge(std::optional<Vec3> target);
  // Whether a strike is under way, and whether its blade is armed.
  [[nodiscard]] bool striking() const noexcept {
    return mode_ == MoveMode::strike;
  }
  [[nodiscard]] bool blade_armed() const noexcept;
  // Drives the robot for the next tick (see ScriptedStep).
  void script(const ScriptedStep &step);
  // Hands the robot back to its controls, standing.
  void release();
  void destroy();

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
  // The twist stick's push this tick (robot +0x2f68), -1..1.
  [[nodiscard]] float twist_push() const noexcept {
    return twist_push_;
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
  // The last tick's steering and forward stick (RobotControl::move).
  [[nodiscard]] const std::array<float, 2> &stick() const noexcept {
    return stick_;
  }
  [[nodiscard]] bool backward() const noexcept {
    return backward_;
  }
  [[nodiscard]] bool grounded() const noexcept {
    return grounded_;
  }
  [[nodiscard]] bool down_boosting() const noexcept {
    return down_boosting_;
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
  [[nodiscard]] std::uint64_t motion_revision() const noexcept {
    return motion_revision_;
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
  void set_mode(MoveMode mode, int motion, bool loops, bool restart = false);
  void tick_ground(const RobotControl &control, float seconds);
  void steer_dash(float side, float seconds);
  void tick_air(const RobotControl &control, float seconds);
  void tick_twist(const RobotControl &control, float seconds);
  void tick_quick_turn(const RobotControl &control, float seconds);
  void tick_strike(float seconds);
  void lunge(float seconds);
  void turn(float stick, float degrees_per_second, float seconds);
  [[nodiscard]] int flick(float stick, float seconds);
  void start_dodge(int side);
  [[nodiscard]] bool tick_dodge(float seconds);

  MovementTuning tuning_;
  Vec3 position_{};
  float heading_{};
  float twist_{};
  float mouse_yaw_{};   // radians the mouse asked to turn, still to turn
  float twist_stick_{}; // the stick's smoothed push, 0..1
  float twist_push_{};
  float twist_reset_from_{};
  float twist_reset_left_{};
  float quick_turn_left_{};     // radians still to turn (+0x2748)
  float quick_turn_cooldown_{}; // seconds (+0x2744)
  float pitch_{};
  MoveMode mode_{MoveMode::stand};
  int motion_{robot_motion::stand};
  std::uint64_t motion_revision_{};
  bool loops_{true};
  float motion_speed_{1.0F};
  float mode_seconds_{};
  bool backward_{};
  bool grounded_{true};
  float floor_{};
  Vec3 local_velocity_{};
  float vertical_speed_{};
  float dash_hold_{};
  float lift_left_{};  // the held jump's lift still to come, seconds
  float lift_spent_{}; // and what it has used, seconds
  bool down_boosting_{};
  bool jump_was_held_{};
  std::array<float, 2> stick_{};
  float stagger_left_{};
  float strike_armed_frames_{};
  float strike_last_frames_{};
  std::optional<Vec3> lunge_at_;
  float lunge_height_{}; // the target's height when the strike began (+0x25b4)
  bool lunge_piloted_{};
  int dodged_{};
  int flick_state_{};  // +0x2538
  float flick_left_{}; // +0x253c, seconds
  const std::vector<MotionTiming> *timings_{};
  int leg_type_{-1};
  float dash_steer_{};    // +0x257c, the eased steer that leans a dash
  Vec3 dodge_velocity_{}; // the push in the robot's frame (+0x256c)
  float dodge_slide_frames_{};
  float dodge_ease_frames_{}; // the motion's loop end (+0x211c) ...
  float dodge_last_frames_{}; // ... and its last frame (+0x2120)
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
