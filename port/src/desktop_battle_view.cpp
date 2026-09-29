#include "desktop_battle_view.hpp"

#include <algorithm>
#include <array>
#include <cmath>
#include <string>

namespace phantomcrash {
namespace {

// Frames a changed motion fades over.
constexpr float motion_fade_frames = 6.0F;

// The node of mesh `mesh` in a converted model ("mesh<m>_node0"), or -1.
int mesh_node(const ModelData *model, const int mesh) {
  if (model == nullptr || mesh < 0) {
    return -1;
  }
  const std::string name = "mesh" + std::to_string(mesh) + "_node0";
  for (std::size_t node = 0; node < model->nodes.size(); ++node) {
    if (model->nodes[node].name == name) {
      return static_cast<int>(node);
    }
  }
  return -1;
}

} // namespace

bool project_point(const Mat4 &view_projection, const Vec3 point, const int width, const int height,
                   ScreenPoint &out) {
  const auto &m = view_projection;
  const float x = point.x * m.at(0, 0) + point.y * m.at(1, 0) + point.z * m.at(2, 0) + m.at(3, 0);
  const float y = point.x * m.at(0, 1) + point.y * m.at(1, 1) + point.z * m.at(2, 1) + m.at(3, 1);
  const float w = point.x * m.at(0, 3) + point.y * m.at(1, 3) + point.z * m.at(2, 3) + m.at(3, 3);
  if (w <= 0.05F) {
    return false;
  }
  out.x = (x / w * 0.5F + 0.5F) * static_cast<float>(width);
  out.y = (0.5F - y / w * 0.5F) * static_cast<float>(height);
  out.depth = w;
  return true;
}

std::unique_ptr<BattleView::Visual> BattleView::make_visual(const RobotParts &parts) const {
  auto visual = std::make_unique<Visual>();
  visual->assembly = assemble_robot(robots_.catalogue(), robots_.skeletons(), parts);
  visual->animator = std::make_unique<RobotAnimator>(robots_, visual->assembly);
  visual->animator->play(robot_motion::stand, true, 0.0F);
  visual->parts = std::make_unique<RobotVisual>(world_, visual->assembly);
  visual->ground_node = mesh_node(world_.model_data(ground_model_), visual->assembly.shadow_mesh);
  return visual;
}

void BattleView::add(const RobotParts &parts) {
  visuals_.push_back(make_visual(parts));
}

void BattleView::replace(const std::size_t index, const RobotParts &parts) {
  if (index < visuals_.size()) {
    visuals_[index] = make_visual(parts);
  }
}

MovementTuning BattleView::movement(const std::size_t index, const MovementTuning &base) const {
  if (index >= visuals_.size()) {
    return base;
  }
  return stride_tuning(robots_, visuals_[index]->assembly, base);
}

float BattleView::radius(const std::size_t index, const float fallback) const {
  if (index >= visuals_.size() || visuals_[index]->assembly.radius <= 0.0F) {
    return fallback;
  }
  return visuals_[index]->assembly.radius;
}

int BattleView::legs_model(const std::size_t index) const {
  return index < visuals_.size() ? visuals_[index]->assembly.leg_type : -1;
}

std::optional<Vec3> BattleView::torso(const std::size_t index) const {
  if (index >= visuals_.size()) {
    return std::nullopt;
  }
  return visuals_[index]->parts->torso(visuals_[index]->pose);
}

void BattleView::set_shield(const EntryShieldLook &look) {
  shield_ = look;
  shield_model_ = world_.model(look.model);
}

void BattleView::set_shadows(const RobotShadowLook &look, const bool split_screen) {
  shadows_ = look;
  split_screen_ = split_screen;
  ground_model_ = look.ground_model.empty() ? -1 : world_.model(look.ground_model);
  for (const auto &visual : visuals_) {
    visual->ground_node = mesh_node(world_.model_data(ground_model_), visual->assembly.shadow_mesh);
  }
}

void BattleView::add_shadow(const Combatant &robot, const Visual &visual, const Mat4 &root,
                            const std::size_t first, std::vector<WorldItem> &items) const {
  switch (robot_shadow(shadows_, robot.piloted, split_screen_, robot.mover.grounded())) {
  case RobotShadow::volume:
    for (std::size_t item = first; item < items.size(); ++item) {
      items[item].casts_shadow = true;
    }
    break;
  case RobotShadow::ground:
    if (ground_model_ >= 0 && visual.ground_node >= 0) {
      WorldItem shadow;
      shadow.model = ground_model_;
      shadow.node = visual.ground_node;
      shadow.world = root;
      shadow.colour = shadows_.ground_colour;
      shadow.double_sided = true;
      items.push_back(std::move(shadow));
    }
    break;
  case RobotShadow::none:
    break;
  }
}

void BattleView::tick(const Battle &battle, const float seconds) {
  shield_spin_ = std::fmod(shield_spin_ + shield_.spin_per_second * seconds, 2.0F * pi);
  const auto &robots = battle.robots();
  for (std::size_t index = 0; index < visuals_.size() && index < robots.size(); ++index) {
    RobotAnimator &animator = *visuals_[index]->animator;
    const RobotMover &mover = robots[index].mover;
    if (animator.motion() != mover.motion()) {
      animator.play(mover.motion(), mover.motion_loops(), motion_fade_frames);
    } else if (!mover.motion_loops() && animator.looping()) {
      animator.release();
    }
    animator.set_speed(mover.motion_speed());
    animator.tick(seconds);
    const Mat4 root = rotation_y(mover.heading()) * translation(mover.position());
    const bool sway = mover.mode() == MoveMode::stand || mover.mode() == MoveMode::walk;
    animator.pose(root, mover.twist(), mover.pitch(), sway, visuals_[index]->pose);
  }
}

void BattleView::add_items(const Battle &battle, std::vector<WorldItem> &items, const int hidden) {
  const auto &robots = battle.robots();
  for (std::size_t index = 0; index < visuals_.size() && index < robots.size(); ++index) {
    const Combatant &robot = robots[index];
    if (!robot.alive || static_cast<int>(index) == hidden) {
      continue;
    }
    Visual &visual = *visuals_[index];
    const RobotMover &mover = robot.mover;
    const Mat4 root = rotation_y(mover.heading()) * translation(mover.position());
    const std::size_t first = items.size();
    visual.parts->add_items(visual.pose, root, items);
    const float flash = robot.hit_flash > 0.0F ? 1.0F : 0.0F;
    std::optional<OpticalSphere> optical;
    if (robot.camouflage_sweep > 0.0F) {
      // 0x074e80 measures from robot +0x454c, a copy of +0x2790 (0x077406):
      // taken, as for the gates, as the lower collision sphere's centre.
      optical = optical_sphere(optical_, robot.aim_origin(), robot.camouflage_sweep);
    }
    for (std::size_t item = first; item < items.size(); ++item) {
      items[item].optical = optical;
      items[item].flash = flash;
    }
    add_shadow(robot, visual, root, first, items);
    add_shield(robot, items);
  }
}

// 0x06f592: the sphere around the robot's lower collision sphere, turning,
// its colour's alpha times the glow, added to what is behind it.
void BattleView::add_shield(const Combatant &robot, std::vector<WorldItem> &items) const {
  if (shield_model_ < 0 || robot.shield_glow <= 0.0F) {
    return;
  }
  WorldItem shell;
  shell.model = shield_model_;
  const float size = robot.radius * shield_.scale;
  shell.world = scaling({size, size, size}) * rotation_y(robot.mover.heading() + shield_spin_) *
                translation(robot.aim_origin());
  shell.colour = shield_.colour;
  shell.alpha = robot.shield_glow;
  shell.additive = true;
  items.push_back(std::move(shell));
}

} // namespace phantomcrash
