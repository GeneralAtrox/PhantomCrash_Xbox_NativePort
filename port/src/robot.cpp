#include "phantomcrash/robot.hpp"

#include <algorithm>
#include <stdexcept>

namespace phantomcrash {
namespace {

int integer(const json::JsonValue &value) {
  return value.is_number() ? static_cast<int>(value.number()) : -1;
}

std::vector<int> integers(const json::JsonValue *array) {
  std::vector<int> out;
  if (array != nullptr && array->is_array()) {
    for (const json::JsonValue &value : array->array()) {
      out.push_back(integer(value));
    }
  }
  return out;
}

std::array<int, 2> pair_of(const json::JsonValue *array) {
  const std::vector<int> values = integers(array);
  return {values.size() > 0 ? values[0] : -1, values.size() > 1 ? values[1] : -1};
}

const json::JsonValue::Array &list(const json::JsonValue &document, const char *key) {
  const json::JsonValue *value = document.find(key);
  if (value == nullptr || !value->is_array()) {
    throw std::runtime_error(std::string("Robot catalogue has no ") + key);
  }
  return value->array();
}

template <typename T>
const T &pick(const std::vector<T> &table, const int index, const char *what) {
  if (index < 0 || index >= static_cast<int>(table.size())) {
    throw std::runtime_error(std::string("No robot ") + what + " " + std::to_string(index));
  }
  return table[static_cast<std::size_t>(index)];
}

} // namespace

RobotCatalogue parse_robot_catalogue(const json::JsonValue &document) {
  RobotCatalogue catalogue;
  const json::JsonValue *parts = document.find("parts");
  if (parts == nullptr || !parts->is_object()) {
    throw std::runtime_error("Robot catalogue has no parts");
  }
  catalogue.body_parts = integers(parts->find("body"));
  catalogue.leg_parts = integers(parts->find("legs"));
  catalogue.arm_parts = integers(parts->find("arm"));
  catalogue.shoulder_parts = integers(parts->find("shoulder"));
  for (const json::JsonValue &value : list(document, "body_types")) {
    catalogue.body_types.push_back({static_cast<int>(value.number_or("model", -1)),
                                    static_cast<int>(value.number_or("emblem_model", -1)),
                                    static_cast<int>(value.number_or("motions", -1)),
                                    static_cast<float>(value.number_or("forearm_turn", 0.0))});
  }
  for (const json::JsonValue &value : list(document, "leg_types")) {
    RobotCatalogue::LegType legs;
    legs.model = static_cast<int>(value.number_or("model", -1));
    legs.motions = static_cast<int>(value.number_or("motions", -1));
    if (const json::JsonValue *arms = value.find("arm_motions"); arms && arms->is_array()) {
      for (std::size_t side = 0; side < 2 && side < arms->array().size(); ++side) {
        legs.arm_motions[side] = pair_of(&arms->array()[side]);
      }
    }
    legs.paints = integers(value.find("paints"));
    if (const json::JsonValue *timings = value.find("timings"); timings && timings->is_array()) {
      for (const json::JsonValue &entry : timings->array()) {
        const auto &f = entry.array();
        if (f.size() == 4) {
          legs.timings.push_back(
              {static_cast<float>(f[0].number()), static_cast<float>(f[1].number()),
               static_cast<float>(f[2].number()), static_cast<float>(f[3].number())});
        }
      }
    }
    legs.radius = static_cast<float>(value.number_or("radius", 0.0));
    legs.shadow_mesh = static_cast<int>(value.number_or("shadow_mesh", -1.0));
    catalogue.leg_types.push_back(std::move(legs));
  }
  for (const json::JsonValue &value : list(document, "arm_types")) {
    catalogue.arm_types.push_back(
        {pair_of(value.find("models")), static_cast<int>(value.number_or("class", 0))});
  }
  for (const json::JsonValue &value : list(document, "shoulder_types")) {
    catalogue.shoulder_types.push_back({pair_of(value.find("models"))});
  }
  catalogue.emblems = integers(document.find("emblems"));
  return catalogue;
}

RobotParts parse_robot_parts(const json::JsonValue &value) {
  RobotParts parts;
  parts.body = static_cast<int>(value.number_or("body", 0));
  parts.legs = static_cast<int>(value.number_or("legs", 0));
  parts.arms = pair_of(value.find("arms"));
  parts.shoulders = pair_of(value.find("shoulders"));
  parts.colour = static_cast<int>(value.number_or("colour", 0));
  parts.emblem = static_cast<int>(value.number_or("emblem", -1));
  return parts;
}

std::string robot_model_path(const int resource) {
  return "models/binlink/" + std::to_string(resource) + ".gltf";
}

std::string robot_texture_path(const int resource) {
  return "textures/binlink/" + std::to_string(resource) + ".png";
}

std::string motion_bank_path(const int resource) {
  return "motions/" + std::to_string(resource) + ".json";
}

RobotAssembly assemble_robot(const RobotCatalogue &catalogue, const RobotSkeletons &skeletons,
                             const RobotParts &parts) {
  RobotAssembly robot;
  const int body_type = pick(catalogue.body_parts, parts.body, "body part");
  const auto &body = pick(catalogue.body_types, body_type, "body type");
  robot.leg_type = pick(catalogue.leg_parts, parts.legs, "leg part");
  const auto &legs = pick(catalogue.leg_types, robot.leg_type, "leg type");
  robot.body_model = robot_model_path(body.model);
  robot.forearm_turn = body.forearm_turn;
  robot.legs_model = robot_model_path(legs.model);
  robot.leg_motions = motion_bank_path(legs.motions);
  robot.timings = legs.timings;
  // 0x080e41 keeps the radius within 0..2.
  robot.radius = std::clamp(legs.radius, 0.0F, max_robot_radius);
  robot.shadow_mesh = legs.shadow_mesh;
  if (!legs.paints.empty()) {
    const int colour =
        parts.colour >= 0 && parts.colour < static_cast<int>(legs.paints.size()) ? parts.colour : 0;
    robot.paint = robot_texture_path(legs.paints[static_cast<std::size_t>(colour)]);
  }
  const auto &skeleton = pick(skeletons.legs, robot.leg_type, "leg skeleton");
  robot.skeleton = skeleton ? &*skeleton : nullptr;
  for (std::size_t side = 0; side < 2; ++side) {
    if (parts.arms[side] >= 0) {
      const int arm_type = pick(catalogue.arm_parts, parts.arms[side], "arm part");
      const auto &arm = pick(catalogue.arm_types, arm_type, "arm type");
      robot.arm_models[side] = robot_model_path(arm.models[side]);
      const int bank = legs.arm_motions[side][arm.arm_class == 0 ? 0 : 1];
      robot.arm_motions[side] = motion_bank_path(bank);
      const auto &arm_skeleton = pick(skeletons.arms[side], arm_type, "arm skeleton");
      robot.arm_skeletons[side] = arm_skeleton ? &*arm_skeleton : nullptr;
    }
    if (parts.shoulders[side] >= 0) {
      const int type = pick(catalogue.shoulder_parts, parts.shoulders[side], "shoulder part");
      if (type >= 0) {
        const auto &shoulder = pick(catalogue.shoulder_types, type, "shoulder type");
        robot.shoulder_models[side] = robot_model_path(shoulder.models[side]);
      }
    }
  }
  return robot;
}

void skin_nodes(const ModelData &model, const PosePart &bones, const int first_slot,
                const Mat4 &fallback, std::vector<Mat4> &node_worlds) {
  node_worlds.resize(model.nodes.size());
  for (std::size_t i = 0; i < model.nodes.size(); ++i) {
    const int slot = first_slot + static_cast<int>(i);
    if (slot >= 0 && slot < bone_slots && bones.set[static_cast<std::size_t>(slot)]) {
      node_worlds[i] = model.nodes[i].local * bones.bones[static_cast<std::size_t>(slot)];
    } else {
      node_worlds[i] = model.nodes[i].world * fallback;
    }
  }
}

} // namespace phantomcrash
