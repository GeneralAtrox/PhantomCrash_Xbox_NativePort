#include "phantomcrash/source_arm_aim.hpp"
#include "phantomcrash/skeleton.hpp"
#include "phantomcrash/rigid_animation.hpp"

#include <algorithm>
#include <cmath>
#include <stdexcept>

namespace phantomcrash {
namespace {

Vec3 row_of(const Mat4 &m, const int row) {
  return {m.at(row, 0), m.at(row, 1), m.at(row, 2)};
}

void set_row(Mat4 &m, const int row, const Vec3 v) {
  m.at(row, 0) = v.x;
  m.at(row, 1) = v.y;
  m.at(row, 2) = v.z;
}

Mat4 rotation_part(const Mat4 &m) {
  Mat4 out;
  for (int row = 0; row < 3; ++row) {
    set_row(out, row, normalize(row_of(m, row)));
  }
  return out;
}

float determinant(const Mat4 &m) {
  return dot(row_of(m, 0), cross(row_of(m, 1), row_of(m, 2)));
}

// The source's axes: the model export mirrors y. Its own inverse.
Vec3 source_axes(const Vec3 v) {
  return {v.x, -v.y, v.z};
}

// The model data's axes: the export mirrors y and the port's loader mirrors
// z (rigid_animation.cpp), a half turn about x. Its own inverse.
Vec3 model_axes(const Vec3 v) {
  return {v.x, -v.y, -v.z};
}

// A source turn about one axis, conjugated by a z mirror for the port's row
// vectors: it reverses the turns about x and y and keeps the one about z.
// Measured in game: with every angle reversed, high-roll rows swung the arms
// 70-90 degrees off the target; this convention keeps them within 10-25.
Mat4 port_turn(const char axis, const float angle) {
  switch (axis) {
  case 'x':
    return rotation_x(-angle);
  case 'y':
    return rotation_y(-angle);
  case 'z':
    return rotation_z(angle);
  default:
    throw std::runtime_error("Source arm aim axis must be x, y or z");
  }
}

Mat4 node_world(const std::vector<RigidNode> &nodes, const int index, const Mat4 &root) {
  const RigidNode &node = nodes[static_cast<std::size_t>(index)];
  return node.local * (node.parent >= 0 ? node_world(nodes, node.parent, root) : root);
}

// The yaw of `frame` alone: its forward (z) row laid flat, with y up.
Mat4 heading_frame(const Mat4 &frame) {
  Vec3 forward = row_of(frame, 2);
  forward.y = 0.0F;
  if (length(forward) < 1e-5F) {
    const Vec3 across = row_of(frame, 0);
    forward = {-across.z, 0.0F, across.x};
  }
  forward = normalize(forward);
  Mat4 out;
  set_row(out, 0, cross({0.0F, 1.0F, 0.0F}, forward));
  set_row(out, 2, forward);
  return out;
}

float number(const json::JsonValue &value, const char *key) {
  const json::JsonValue *found = value.find(key);
  if (found == nullptr || !found->is_number()) {
    throw std::runtime_error(std::string("source_arm_aim: needs a number \"") + key + "\"");
  }
  return static_cast<float>(found->number());
}

} // namespace

std::optional<SourceArmAim> parse_source_arm_aim(const json::JsonValue *value) {
  if (value == nullptr || !value->is_object()) {
    return std::nullopt;
  }
  SourceArmAim out;
  const json::JsonValue *sides = value->find("sides");
  if (sides == nullptr || !sides->is_array() || sides->array().size() != 2) {
    throw std::runtime_error("source_arm_aim: needs two \"sides\"");
  }
  for (std::size_t side = 0; side < 2; ++side) {
    const json::JsonValue &entry = sides->array()[side];
    SourceArmAimSide &row = out.sides[side];
    row.node = static_cast<int>(number(entry, "node"));
    row.pitch = number(entry, "pitch");
    row.yaw = number(entry, "yaw");
    row.roll = number(entry, "roll");
    row.twist = static_cast<float>(entry.number_or("twist", 0.0));
    const json::JsonValue *limits = entry.find("pitch_limits");
    if (limits == nullptr || !limits->is_array() || limits->array().size() != 2) {
      throw std::runtime_error("source_arm_aim: needs two \"pitch_limits\"");
    }
    row.pitch_limits = {static_cast<float>(limits->array()[0].number()),
                        static_cast<float>(limits->array()[1].number())};
  }
  if (const json::JsonValue *order = value->find("order"); order && order->is_array()) {
    for (const json::JsonValue &axis : order->array()) {
      const std::string &name = axis.string();
      if (name.size() != 1 || name[0] < 'x' || name[0] > 'z') {
        throw std::runtime_error("source_arm_aim: an \"order\" axis must be x, y or z");
      }
      out.order.push_back(name[0]);
    }
  }
  out.keep_rotation = value->boolean_or("keep_rotation", true);
  return out;
}

void aim_source_arm(const SourceArmAim &aim, const std::size_t side, const ArmAimState &state,
                    const Mat4 &body, std::vector<RigidNode> &nodes) {
  const SourceArmAimSide &row = aim.sides.at(side);
  if (row.node < 0 || row.node >= static_cast<int>(nodes.size()) || state.weight <= 0.0F) {
    return;
  }
  const float weight = std::min(state.weight, 1.0F);
  // 0x4e7b40: the robot's offset from the target, in the robot's frame.
  const Mat4 frame = rotation_part(body);
  const Vec3 offset = state.origin - state.target;
  const Vec3 local = source_axes({dot(offset, row_of(frame, 0)), dot(offset, row_of(frame, 1)),
                                  dot(offset, row_of(frame, 2))});
  float pitch = row.pitch;
  if (local.z != 0.0F) {
    pitch -= std::atan2(local.y, local.z);
  } else if (local.y != 0.0F) {
    pitch = -pi;
  }
  pitch = std::remainder(pitch, 2.0F * pi);
  pitch = std::min(row.pitch_limits[1], std::max(row.pitch_limits[0], pitch));
  // 0x4e7cb0 scales the angles by the raise and 0x4e7f04 slerps by it again.
  Mat4 turn;
  for (const char axis : aim.order) {
    const float angle = axis == 'x' ? pitch : axis == 'y' ? row.yaw : row.roll;
    turn = turn * port_turn(axis, angle * weight);
  }
  RigidNode &node = nodes[static_cast<std::size_t>(row.node)];
  if (!aim.keep_rotation) {
    // 0x4e7db4..4e7f14: a zero +0x2b flag sets the node's world rotation
    // outright to the turn followed by RotY (0x4e7e78) of the actor's +0x150
    // heading plus its +0x15c, wrapped to a half turn. It cancels the
    // node's whole world rotation (the inverse at 0x4e7ed8 and the products
    // at 0x4e7ee8/0x4e7f14), not just its own, so the legs' sway and the
    // body's tilt do not reach the arm. The body frame's yaw stands in for
    // the heading and the sum; the slerp at 0x4e7f04 weights the change.
    const Mat4 world = rotation_part(node_world(nodes, row.node, body));
    turn = turn * heading_frame(body) * transposed(world);
  }
  node.local = partial_rotation(turn, weight) * node.local;
}

void aim_source_weapon(const int node, const float twist, const ArmAimState &state,
                       const Mat4 &root, std::vector<RigidNode> &nodes) {
  if (node < 0 || node >= static_cast<int>(nodes.size()) || state.weight <= 0.0F) {
    return;
  }
  RigidNode &aimed = nodes[static_cast<std::size_t>(node)];
  const Mat4 world =
      aimed.local * (aimed.parent >= 0 ? node_world(nodes, aimed.parent, root) : root);
  const Vec3 position{world.at(3, 0), world.at(3, 1), world.at(3, 2)};
  // 0x4e7fd0: z away from the target, x = up x z with up (0, 1, 0), in the
  // model data's axes (the loader's z mirror on top of the export's y one).
  const Vec3 z = normalize(model_axes(position - state.target));
  const Vec3 across = cross({0.0F, 1.0F, 0.0F}, z);
  if (length(across) < 1e-5F) {
    return;
  }
  const Vec3 x = normalize(across);
  const Vec3 y = normalize(cross(z, x));
  // 0x4e80f4 rolls the frame about its z (the barrel) by the row's twist.
  const float c = std::cos(twist);
  const float s = std::sin(twist);
  // Back in port axes the barrel (the model's +z) faces the target.
  Mat4 desired;
  set_row(desired, 0, model_axes(x * c + y * s));
  set_row(desired, 1, model_axes(y * c - x * s) * -1.0F);
  set_row(desired, 2, model_axes(z) * -1.0F);
  const Mat4 current = rotation_part(world);
  if (determinant(current) < 0.0F) {
    desired = scaling({-1.0F, 1.0F, 1.0F}) * desired;
  }
  aimed.local = partial_rotation(desired * transposed(current), state.weight) * aimed.local;
}

Mat4 partial_rotation(const Mat4 &rotation_matrix, const float weight) {
  if (weight >= 1.0F) {
    Mat4 out = rotation_part(rotation_matrix);
    return out;
  }
  // The quaternion of the column-vector form (the transpose).
  const auto r = [&](const int row, const int column) { return rotation_matrix.at(column, row); };
  float x{};
  float y{};
  float z{};
  float w{};
  const float trace = r(0, 0) + r(1, 1) + r(2, 2);
  if (trace > 0.0F) {
    const float s = std::sqrt(trace + 1.0F) * 2.0F;
    w = s / 4.0F;
    x = (r(2, 1) - r(1, 2)) / s;
    y = (r(0, 2) - r(2, 0)) / s;
    z = (r(1, 0) - r(0, 1)) / s;
  } else if (r(0, 0) > r(1, 1) && r(0, 0) > r(2, 2)) {
    const float s = std::sqrt(1.0F + r(0, 0) - r(1, 1) - r(2, 2)) * 2.0F;
    w = (r(2, 1) - r(1, 2)) / s;
    x = s / 4.0F;
    y = (r(0, 1) + r(1, 0)) / s;
    z = (r(0, 2) + r(2, 0)) / s;
  } else if (r(1, 1) > r(2, 2)) {
    const float s = std::sqrt(1.0F + r(1, 1) - r(0, 0) - r(2, 2)) * 2.0F;
    w = (r(0, 2) - r(2, 0)) / s;
    x = (r(0, 1) + r(1, 0)) / s;
    y = s / 4.0F;
    z = (r(1, 2) + r(2, 1)) / s;
  } else {
    const float s = std::sqrt(1.0F + r(2, 2) - r(0, 0) - r(1, 1)) * 2.0F;
    w = (r(1, 0) - r(0, 1)) / s;
    x = (r(0, 2) + r(2, 0)) / s;
    y = (r(1, 2) + r(2, 1)) / s;
    z = s / 4.0F;
  }
  if (w < 0.0F) {
    x = -x;
    y = -y;
    z = -z;
    w = -w;
  }
  const float half = std::acos(std::min(w, 1.0F));
  const float sine = std::sin(half);
  if (sine < 1e-6F || weight <= 0.0F) {
    return Mat4{};
  }
  const float scale = std::sin(half * weight) / sine;
  return rotation(x * scale, y * scale, z * scale, std::cos(half * weight));
}
} // namespace phantomcrash
