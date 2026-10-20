// The S.L.A.I. carrier's own flight (PAL battledvd.bin): its update rate,
// speeds and counts, the wing swing around the drop and the wreck's pose.
#include "phantomcrash/supply_drop.hpp"

#include <algorithm>
#include <cmath>

namespace phantomcrash {

// A stage with a source flight runs the drop on the source's values; the
// settings' own stand for Phantom Crash's carrier.
void SupplyDrop::use_flight() {
  const SupplySourceFlight *f = flight();
  if (f == nullptr) {
    return;
  }
  SupplySettings &s = settings_;
  s.tick_rate = f->update_rate;
  s.speed = f->speed;
  s.drop_ticks = f->release_delay;
  s.hatch_open = f->hatch_open;
  s.hatch_speed = f->hatch_speed;
  s.spin_speed = f->light_step;
  s.hold = {}; // 0x524fc0 state 0: a carried crate sits at the carrier's origin
  s.fall_speed = f->fall;
  s.rest_height = f->rest;
  s.lifetime_ticks = f->lifetime;
  s.carrier_radius = f->carrier_radius;
  s.pickup_radius = f->pickup_radius;
  s.wreck_keep = f->wreck_keep;
  s.wreck_sink = f->wreck_sink;
  s.wreck_ticks = f->wreck_updates;
}

// 0x524500: while the counter is short of 1 / speed, each update turns
// joints 0 and 1 by their swing over 1 / speed, opening (+1) or closing (-1).
void SupplyDrop::swing_wings(const float ticks) {
  const SupplySourceFlight *f = flight();
  if (f == nullptr || wing_sign_ == 0.0F) {
    return;
  }
  const float limit = std::ceil(1.0F / f->speed);
  const float step = std::min(ticks, limit - wing_count_);
  if (step <= 0.0F) {
    return;
  }
  wing_count_ += step;
  for (std::size_t joint = 0; joint < f->wing_swing.size(); ++joint) {
    carrier_.joints[joint] =
        carrier_.joints[joint] + f->wing_swing[joint] * (wing_sign_ * f->speed * step);
  }
}

// 0x524ad0: each joint angle approaches the fall's pose by wreck_ease an
// update.
void SupplyDrop::wreck_pose(const float ticks) {
  const SupplySourceFlight *f = flight();
  if (f == nullptr) {
    return;
  }
  const float most = f->wreck_ease * ticks;
  const auto ease = [most](const float from, const float to) {
    return from + std::clamp(to - from, -most, most);
  };
  for (std::size_t joint = 0; joint < carrier_.joints.size(); ++joint) {
    Vec3 &angles = carrier_.joints[joint];
    const Vec3 &pose = f->wreck_pose[joint];
    angles = {ease(angles.x, pose.x), ease(angles.y, pose.y), ease(angles.z, pose.z)};
  }
}

} // namespace phantomcrash
