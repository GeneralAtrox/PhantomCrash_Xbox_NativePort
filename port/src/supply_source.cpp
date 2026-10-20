// The S.L.A.I. supply presentation and flight (stage "supply_pickups").
#include "phantomcrash/supply_source.hpp"

#include "phantomcrash/json_fields.hpp"
#include "phantomcrash/skeleton.hpp"
#include "phantomcrash/world_model.hpp"

#include <stdexcept>
#include <string>

namespace phantomcrash {

namespace {

using json::JsonValue;

Vec3 vec3(const JsonValue &value) {
  const auto v = json::numbers<3>(value, "supply joint angles");
  return {v[0], v[1], v[2]};
}

template <std::size_t N> std::array<Vec3, N> vec3s(const JsonValue &value, const char *key) {
  const auto &list = json::required(value, key).array();
  if (list.size() != N) {
    throw std::runtime_error(std::string("supply flight ") + key + " has the wrong length");
  }
  std::array<Vec3, N> out{};
  for (std::size_t index = 0; index < N; ++index) {
    out[index] = vec3(list[index]);
  }
  return out;
}

} // namespace

SupplySourceFlight parse_supply_source_flight(const JsonValue &value) {
  const auto number = [&](const char *key) {
    return static_cast<float>(json::required(value, key).number());
  };
  SupplySourceFlight f;
  f.update_rate = number("update_rate");
  f.speed = number("speed");
  f.release_delay = number("release_delay");
  f.lifetime = number("lifetime");
  f.burst_hold = number("burst_hold");
  f.fall = number("fall");
  f.rest = number("rest");
  f.hatch_open = number("hatch_open");
  f.hatch_speed = number("hatch_speed");
  f.light_step = number("light_step");
  f.flight_light_step = number("flight_light_step");
  const auto &controls = json::required(value, "joint_controls").array();
  if (controls.size() != f.joint_controls.size()) {
    throw std::runtime_error("supply flight joint_controls has the wrong length");
  }
  for (std::size_t index = 0; index < controls.size(); ++index) {
    f.joint_controls[index] = static_cast<int>(controls[index].number());
  }
  f.wing_swing = vec3s<2>(value, "wing_swing");
  f.wreck_pose = vec3s<supply_joint_count>(value, "wreck_pose");
  f.wreck_ease = number("wreck_ease");
  f.wreck_keep = number("wreck_keep");
  f.wreck_sink = number("wreck_sink");
  f.wreck_accel = number("wreck_accel");
  f.wreck_updates = number("wreck_updates");
  f.wreck_shade = number("wreck_shade");
  f.carrier_radius = number("carrier_radius");
  f.pickup_radius = number("pickup_radius");
  f.respawn = number("respawn");
  if (f.update_rate <= 0 || f.speed <= 0 || f.hatch_speed <= 0) {
    throw std::runtime_error("Invalid supply flight rates");
  }
  return f;
}

SupplyPickupLook parse_supply_pickup_look(const JsonValue &value) {
  SupplyPickupLook look;
  look.model = json::required(value, "model").string();
  look.scale = static_cast<float>(value.number_or("scale", look.scale));
  look.carrier_model = value.string_or("carrier_model", "");
  look.carrier_light_model = value.string_or("carrier_light_model", "");
  look.light_spin_per_second = static_cast<float>(value.number_or("light_spin_per_second", 0));
  if (const auto *pulse = value.find("pulse")) {
    const auto number = [&](const char *key) {
      return static_cast<float>(json::required(*pulse, key).number());
    };
    look.pulse =
        SupplyPickupPulse{number("frame_rate"),   number("step"),           number("fast_frames"),
                          number("fast_rate"),    number("life_numerator"), number("colour_base"),
                          number("colour_steps"), number("colour_divisor")};
    if (look.pulse->frame_rate <= 0 || look.pulse->colour_divisor <= 0) {
      throw std::runtime_error("Invalid supply pickup pulse");
    }
  }
  if (const auto *flight = value.find("flight")) {
    look.flight = parse_supply_source_flight(*flight);
  }
  const auto &images = json::required(value, "images").array();
  if (images.size() != look.images.size()) {
    throw std::runtime_error("Supply pickup images must cover all ten kinds");
  }
  for (std::size_t i = 0; i < images.size(); ++i) {
    look.images[i] = images[i].string();
  }
  return look;
}

Mat4 supply_joint_rotation(const Vec3 angles) {
  return rotation_z(angles.z) * rotation_x(angles.x) * rotation_y(angles.y);
}

std::vector<Mat4> supply_carrier_nodes(const ModelData &model, const SupplySourceFlight &flight,
                                       const SupplyJoints &joints, const Mat4 &world) {
  // The importer names each control source_node_N; control 0, the root,
  // holds no mesh and turns everything under it.
  Mat4 root = Mat4{};
  std::vector<Mat4> turns(model.nodes.size(), Mat4{});
  for (std::size_t joint = 0; joint < joints.size(); ++joint) {
    const int control = flight.joint_controls[joint];
    if (control == 0) {
      root = supply_joint_rotation(joints[joint]);
      continue;
    }
    const std::string name = "source_node_" + std::to_string(control);
    for (std::size_t node = 0; node < model.nodes.size(); ++node) {
      if (model.nodes[node].name == name) {
        turns[node] = supply_joint_rotation(joints[joint]);
      }
    }
  }
  std::vector<Mat4> local(model.nodes.size());
  std::vector<Mat4> out(model.nodes.size());
  for (std::size_t node = 0; node < model.nodes.size(); ++node) {
    const ModelNode &entry = model.nodes[node];
    const Mat4 above = entry.parent >= 0 ? local[static_cast<std::size_t>(entry.parent)] : root;
    local[node] = turns[node] * entry.parent_local * above;
    out[node] = local[node] * world;
  }
  return out;
}

} // namespace phantomcrash
