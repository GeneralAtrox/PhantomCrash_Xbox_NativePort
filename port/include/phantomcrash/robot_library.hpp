#pragma once

#include "phantomcrash/data_roots.hpp"
#include "phantomcrash/motion.hpp"
#include "phantomcrash/robot.hpp"
#include "phantomcrash/robot_mover.hpp"
#include "phantomcrash/skeleton.hpp"

#include <array>
#include <map>
#include <memory>
#include <string>
#include <vector>

namespace phantomcrash {

// The converted robot tables, skeletons and motion banks, loaded once from
// the asset roots (so a mod can replace any of them).
class RobotLibrary final {
public:
  explicit RobotLibrary(const DataRoots &roots);

  // False when the robots have not been converted (see errors()).
  [[nodiscard]] bool available() const noexcept {
    return available_;
  }
  [[nodiscard]] const RobotCatalogue &catalogue() const noexcept {
    return catalogue_;
  }
  [[nodiscard]] const RobotSkeletons &skeletons() const noexcept {
    return skeletons_;
  }
  // The bank at `asset_path`, loaded on first use; null when it cannot load.
  [[nodiscard]] const MotionBank *bank(const std::string &asset_path);
  // What failed to load, oldest first.
  [[nodiscard]] const std::vector<std::string> &errors() const noexcept {
    return errors_;
  }

private:
  const DataRoots &roots_;
  bool available_{};
  RobotCatalogue catalogue_;
  RobotSkeletons skeletons_;
  std::map<std::string, std::unique_ptr<MotionBank>> banks_;
  std::vector<std::string> errors_;
};

// Plays one robot's motions: the leg bank drives the body and legs, the two
// arm banks play the same motion id beside it (0x08ddb2), and a change of
// motion fades over a few frames.
class RobotAnimator final {
public:
  RobotAnimator(RobotLibrary &library, const RobotAssembly &assembly);

  // Starts `motion` (a bank index) at `start_frame`, looping or holding its
  // last frame. Asking for the playing motion again only changes `loop`.
  void play(int motion, bool loop, float fade_frames = 8.0F, float start_frame = 0.0F);
  // Plays on past the loop to the last frame (a walk's stop, a landing).
  void release() noexcept {
    loop_ = false;
  }
  // Frames advance `speed` times the motion rate (the walk slows with a
  // half-pressed stick, 0x08e6b6).
  void set_speed(float speed) noexcept {
    speed_ = speed;
  }
  void tick(double seconds);

  [[nodiscard]] int motion() const noexcept {
    return motion_;
  }
  [[nodiscard]] bool finished() const noexcept;
  [[nodiscard]] bool looping() const noexcept {
    return loop_;
  }
  [[nodiscard]] float frame() const noexcept {
    return frame_;
  }

  // Poses the robot standing at `root` (its ground point and heading).
  void pose(const Mat4 &root, float aim_yaw, float aim_pitch, bool keep_sway, Pose &out) const;

private:
  [[nodiscard]] MotionTiming timing(int motion) const;
  [[nodiscard]] MotionSource source(const MotionBank *bank) const;

  const RobotAssembly &assembly_; // outlives the animator
  const MotionBank *legs_{};
  std::array<const MotionBank *, 2> arms_{};
  int motion_{-1};
  bool loop_{};
  float frame_{};
  int previous_motion_{-1};
  float previous_frame_{};
  float speed_{1.0F};
  float fade_{1.0F}; // 0..1 weight of the current motion
  float fade_frames_{1.0F};
};

// The playback frame rate of motion key frames.
inline constexpr float motion_frames_per_second = 60.0F;

// Units per second the body travels over `motion`'s loop: the root-motion
// track (the skeleton node flagged 0x2) that moves robots in the original
// (0x0910d4). 0 when the legs have no root motion (hovering drones).
[[nodiscard]] float stride_speed(const MotionBank &bank, const RobotAssembly &assembly, int motion);

// `base` with the walk and dash speeds the robot's leg motions stride at,
// so its feet stay planted and each leg part keeps its own pace; speeds the
// motions do not give stay as `base` has them.
[[nodiscard]] MovementTuning stride_tuning(RobotLibrary &library, const RobotAssembly &assembly,
                                           MovementTuning base);

} // namespace phantomcrash
