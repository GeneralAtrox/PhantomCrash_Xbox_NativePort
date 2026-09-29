#include "desktop_robot.hpp"

namespace phantomcrash {
namespace {

int flagged_bone(const SkeletonNode &node, const std::uint32_t flag) {
  if ((node.flags & flag) != 0 && node.bone >= 0) {
    return node.bone;
  }
  for (const SkeletonNode &child : node.children) {
    if (const int bone = flagged_bone(child, flag); bone >= 0) {
      return bone;
    }
  }
  return -1;
}

int load(WorldRenderer &world, const std::string &path) {
  return path.empty() ? -1 : world.model(path);
}

} // namespace

RobotVisual::RobotVisual(WorldRenderer &world, const RobotAssembly &assembly) : world_(world) {
  body_ = load(world, assembly.body_model);
  legs_ = load(world, assembly.legs_model);
  paint_ = world.texture(assembly.paint);
  if (assembly.skeleton != nullptr) {
    torso_slot_ = flagged_bone(*assembly.skeleton, node_turns_with_aim);
  }
  const ModelData *body = world.model_data(body_);
  const int body_bones = body != nullptr ? static_cast<int>(body->nodes.size()) : 0;
  parts_.push_back({body_, 0, 0, {}});
  parts_.push_back({legs_, 0, body_bones, {}});
  for (std::size_t side = 0; side < 2; ++side) {
    parts_.push_back({load(world, assembly.arm_models[side]), static_cast<int>(side) + 1, 0, {}});
    shoulders_.push_back({load(world, assembly.shoulder_models[side]), 0, 0, {}});
  }
  for (const std::vector<Part> *parts : {&parts_, &shoulders_}) {
    for (const Part &part : *parts) {
      world.prepare_shadow(part.model);
    }
  }
}

void RobotVisual::add(Part &part, const Pose &pose, const Mat4 &root,
                      std::vector<WorldItem> &items) {
  const ModelData *model = world_.model_data(part.model);
  if (model == nullptr) {
    return;
  }
  skin_nodes(*model, pose[static_cast<std::size_t>(part.pose_part)], part.first_slot, root,
             part.node_worlds);
  WorldItem item;
  item.model = part.model;
  item.world = root;
  item.node_worlds = part.node_worlds;
  item.paint = paint_;
  items.push_back(std::move(item));
}

void RobotVisual::add_items(const Pose &pose, const Mat4 &root, std::vector<WorldItem> &items) {
  for (Part &part : parts_) {
    add(part, pose, root, items);
  }
  const PosePart &body = pose[0];
  const bool torso = torso_slot_ >= 0 && body.set[static_cast<std::size_t>(torso_slot_)];
  const Mat4 mount = torso ? body.bones[static_cast<std::size_t>(torso_slot_)] : root;
  for (Part &part : shoulders_) {
    const ModelData *model = world_.model_data(part.model);
    if (model == nullptr) {
      continue;
    }
    WorldItem item;
    item.model = part.model;
    item.world = mount;
    item.paint = paint_;
    items.push_back(std::move(item));
  }
}

std::optional<Vec3> RobotVisual::torso(const Pose &pose) const {
  const PosePart &body = pose[0];
  if (torso_slot_ < 0 || !body.set[static_cast<std::size_t>(torso_slot_)]) {
    return std::nullopt;
  }
  const Mat4 &bone = body.bones[static_cast<std::size_t>(torso_slot_)];
  return Vec3{bone.at(3, 0), bone.at(3, 1), bone.at(3, 2)};
}

} // namespace phantomcrash
