#pragma once

// The S.L.A.I. supply carrier and its crates (PAL battledvd.bin, see
// docs/SLAI_INTEGRATION.md): asset-only presentation plus the flight
// recovered from the source's carrier states. Addresses below are the
// overlay's (loaded at 0x4d0c00).

#include "phantomcrash/json.hpp"
#include "phantomcrash/math3d.hpp"

#include <array>
#include <cstddef>
#include <optional>
#include <string>
#include <vector>

namespace phantomcrash {

struct ModelData;

// The original's kinds, each with its own crate in the carrier models;
// data may append more (SupplySettings::kinds).
inline constexpr int supply_kind_count = 10;

struct SupplyPickupPulse {
  float frame_rate{};
  float step{};
  float fast_frames{};
  float fast_rate{};
  float life_numerator{};
  float colour_base{};
  float colour_steps{};
  float colour_divisor{};
};

// The carrier's seven articulated joints (0x5f9290, stride 16).
inline constexpr std::size_t supply_joint_count = 7;
using SupplyJoints = std::array<Vec3, supply_joint_count>;

// The source carrier's flight. All counts are source updates (update_rate a
// second); lengths are native world units and angles native radians, each
// converted by the importer. The S.L.A.I. update (0x523430) runs once per
// libgf frame, the field rate over interval 2 (ELF 0x1a86e0).
struct SupplySourceFlight {
  float update_rate{};       // 0x1a8788: field rate / 2
  float speed{};             // of a segment an update (0x5226c0 passes it to 0x523e70)
  float release_delay{};     // between crates leaving the hold (0x522d30 +0x10)
  float lifetime{};          // a landed crate's updates (0x522d30 +0x14)
  float burst_hold{};        // a burst crate stays drawn (0x522d30 +0x18)
  float fall{};              // a falling crate's drop an update (0x525058)
  float rest{};              // a crate rests this far above its route's floor
  float hatch_open{};        // the cargo timer's limit (0x5247a0 substate 1)
  float hatch_speed{};       // its step an update, opening and closing
  float light_step{};        // the light's turn every update (0x5235c8)
  float flight_light_step{}; // added by the flight handler (0x5244a4)
  // The model control each joint turns (control 0: the whole carrier).
  std::array<int, supply_joint_count> joint_controls{};
  // Joints 0 and 1 swing by these as the wings open (0x5246a4..0x524788),
  // in 1 / speed updates rounded up (0x524500's counter).
  std::array<Vec3, 2> wing_swing{};
  SupplyJoints wreck_pose{}; // the joints' pose in the fall (0x5b3c20)
  float wreck_ease{};        // a joint angle's approach an update (0x524ad0)
  float wreck_keep{};        // of the speed kept when shot down (0x523ac0)
  float wreck_sink{};        // the shot-down fall's first drop an update
  float wreck_accel{};       // the fall's speed factor an update (0x524ad0)
  float wreck_updates{};     // until the wreck blows up anyway (0x5f8c18)
  float wreck_shade{};       // the shot-down model's colour (0x523ac0)
  float carrier_radius{};    // shot reach (0x523970)
  float pickup_radius{};
  float respawn{}; // updates between the hold emptying and a new route (0x5254d0)
};

// Optional asset-only pickup presentation. Each kind selects its authored
// texture, keeping the model's own UVs and samplers.
struct SupplyPickupLook {
  std::string model;
  std::array<std::string, supply_kind_count> images;
  float scale{2.5F};
  std::string carrier_model;
  std::string carrier_light_model;
  float light_spin_per_second{}; // without a flight
  std::optional<SupplyPickupPulse> pulse;
  std::optional<SupplySourceFlight> flight;
};
[[nodiscard]] SupplyPickupLook parse_supply_pickup_look(const json::JsonValue &value);
[[nodiscard]] SupplySourceFlight parse_supply_source_flight(const json::JsonValue &value);

// A joint's turn (the source's z, x, y order, native signs).
[[nodiscard]] Mat4 supply_joint_rotation(Vec3 angles);

// Each retained node's world matrix with the joints applied about their
// own pivots; children follow their parents. Control 0 turns the carrier.
[[nodiscard]] std::vector<Mat4> supply_carrier_nodes(const ModelData &model,
                                                     const SupplySourceFlight &flight,
                                                     const SupplyJoints &joints, const Mat4 &world);

} // namespace phantomcrash
