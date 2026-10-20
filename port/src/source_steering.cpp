#include "phantomcrash/source_steering.hpp"
#include "phantomcrash/robot_mover.hpp"

#include <algorithm>
#include <cmath>

namespace phantomcrash {

void apply_source_steering(RobotControl &control) {
  if (!control.standard) {
    return;
  }
  // 0x4debd8: atan2 of the turn over the negated aim, measured from the
  // aim's axis; both thresholds are symmetric, so the aim's sign is moot.
  const float angle = std::fabs(std::atan2(control.move[0], -control.pitch));
  if (angle < source_turn_dead_angle || angle > source_turn_dead_far) {
    control.move[0] = 0.0F; // 0x4dec3c
  } else if (angle > source_aim_dead_low && angle < source_aim_dead_high) {
    control.pitch = 0.0F; // 0x4dec88
  }
}

float source_turn_playback_rate(const float steer, const float turn_degrees) {
  const float turn = std::fabs(steer * source_radians_per_degree * turn_degrees *
                               static_cast<float>(source_frame_vsyncs));
  const float rate = (1.0F + turn / source_turn_rate_step) / 2.0F;
  return std::clamp(rate, source_turn_rate_min, source_turn_rate_max);
}

float source_turn_degrees(const MovementTuning &tuning) {
  return tuning.source_turn_degrees > 0.0F
             ? tuning.source_turn_degrees
             : tuning.turn_degrees_per_second / source_vsyncs_per_second;
}

} // namespace phantomcrash
