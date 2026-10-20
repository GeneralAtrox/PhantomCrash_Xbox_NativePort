#pragma once

// S.L.A.I. legs' steering inputs (PAL SLES_529.40): the player's filtered
// turn control (actor +0xe18), the legs' turn speed (+0x8c), the turn
// motion's playback rate and the frame cadence they run at. See
// docs/SLAI_INTEGRATION.md, "Original wheel steering inputs".

#include "phantomcrash/movement_tuning.hpp"

namespace phantomcrash {

struct RobotControl;

// The battle runs one update a game frame of two 60 Hz vsyncs: the display
// object's interval (+0x10, set by 0x1a86e0 from 0x1cbc84 and 0x269494) is
// 2, its rate (+0x14) 60 / 2 in 60 Hz mode.
inline constexpr float source_vsyncs_per_second = 60.0F;
inline constexpr int source_frame_vsyncs = 2;
// 0x4e55f0, 0x4ebc38: the turn speed is in degrees.
inline constexpr float source_radians_per_degree = 0.01745329238474369F; // 0x3c8efa35

// 0x4debd8..4dede0, analog layout 3 (0x2208c8, the S.L.A.I. default from
// 0x2205a0): the right stick's x turns (+0xe18) and its y aims (+0xe2c).
// The stick's angle from its y axis (0x1aa210) drops the turn within 30
// degrees of straight up or down and the aim within 20 degrees of straight
// sideways.
inline constexpr float source_turn_dead_angle = 0.52359879F; // pi/6, 0x3f060a92
inline constexpr float source_turn_dead_far = 2.61799383F;   // 5pi/6, 0x40278d36
inline constexpr float source_aim_dead_low = 1.22173047F;    // 70 degrees, 0x3f9c61aa
inline constexpr float source_aim_dead_high = 1.91986215F;   // 110 degrees, 0x3ff5be0b

// Applies that filter to the Standard controls (the port's layout 3: move[0]
// turns and pitch aims). The pad's own 55-of-128 dead zone and its rescale
// to the full range (0x1b7550, 0x4dec90..4dedd8: minus 0x3edc0000, over
// 0x3f120000) are the host's dead zone (host_controls.hpp), an adapter.
// Clear (layout 2, 0x220880: the right stick's x aims sideways instead)
// and computer robots, which write +0xe18 directly, pass through. The pad
// path has no release filtering: 0x4de760 clears +0xe10..e37 and each
// frame recomputes them, so a let-go stick stops the turn at once.
void apply_source_steering(RobotControl &control);

// 0x4ebc28..4ebcd4: a turning robot's turn motion plays at
// (1 + |steer * turn * vsyncs| / (pi / 40)) / 2 its rate, within 0.9..1.1;
// `turn_degrees` is +0x8c, its degrees a vsync.
inline constexpr float source_turn_rate_step = 0.07853982F; // pi/40, 0x3da0d97c
inline constexpr float source_turn_rate_min = 0.9F;         // 0x3f666666
inline constexpr float source_turn_rate_max = 1.1F;         // 0x3f8ccccd
[[nodiscard]] float source_turn_playback_rate(float steer, float turn_degrees);

// +0x8c for a robot: its S.L.A.I. legs' loaded turn, or else (legs without a
// "source_load", as older registrations) the native turn rate over the
// vsyncs a second, a labelled adapter.
[[nodiscard]] float source_turn_degrees(const MovementTuning &tuning);

} // namespace phantomcrash
