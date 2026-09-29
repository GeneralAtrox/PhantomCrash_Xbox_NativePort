// The training battle's split screen (split_screen.hpp): the further
// screens' robots, chase cameras and HUDs, and one screen's world and HUD.
#include "desktop_training.hpp"

#include <SDL3/SDL.h>

#include <algorithm>

namespace phantomcrash {

void TrainingScreen::build_split(const DataRoots &roots) {
  const int robots = static_cast<int>(battle_->robots().size());
  for (int robot = 1; robot < std::min(screens_, robots); ++robot) {
    splits_.push_back({robot, ChaseCamera(script_.battle.camera),
                       make_battle_hud(textures_, font_, roots, screens_, robot)});
  }
}

void TrainingScreen::tick_split(const float seconds) {
  const StageDoors *doors = battle_->doors();
  for (SplitView &split : splits_) {
    const RobotMover &mover = battle_->robots().at(static_cast<std::size_t>(split.robot)).mover;
    split.camera.tick(mover.position(), mover.aim_heading(), mover.pitch(),
                      battle_->settings().arena.half_size, seconds, !split.placed);
    split.placed = true;
    const CameraShot chase = split.camera.chase();
    if (const auto hit = doors ? doors->segment_hit(chase.target, chase.eye) : std::nullopt) {
      split.camera.block_eye(chase.target, *hit);
    }
  }
}

// Screen 0 is the player's, with the camera views and the cockpit; the
// others follow their robots from behind.
void TrainingScreen::draw_screen(const Viewport &screen, const std::size_t index) {
  SplitView *split = index == 0 ? nullptr : &splits_.at(index - 1);
  const ChaseCamera &camera = split != nullptr ? split->camera : camera_;
  // The world fills the screen, widening the view (Hor+); the HUD and
  // messages stay in the screen's canvas.
  const float aspect =
      static_cast<float>(screen.pixel_width) / static_cast<float>(screen.pixel_height);
  const Mat4 view = camera.view();
  const Mat4 projection = camera.projection(aspect);
  if (world_ != nullptr && world_->available() && stage_.model >= 0) {
    WorldFrame frame;
    frame.width = screen.pixel_width;
    frame.height = screen.pixel_height;
    frame.view = view;
    frame.projection = projection;
    frame.eye = camera.eye();
    frame.lighting = lighting_;
    frame.shadow_length = script_.battle.shadows.volume_length;
    frame.items.push_back(stage_);
    add_door_items(*battle_, door_model_, frame.items);
    if (supply_ && battle_->supply() != nullptr) {
      supply_->add_items(*battle_->supply(), frame.items, screens_ > 1);
      if (screens_ == 1) {
        supply_->add_sprites(*battle_->supply(), frame.eye, frame.sprites);
      }
    }
    if (effects_) {
      effects_->add_sprites(frame.sprites);
    }
    if (shots_) {
      shots_->add_items(*battle_, view, frame.items);
      shots_->add_sprites(*battle_, effects_->system(), frame.sprites);
    }
    if (view_) {
      view_->add_items(*battle_, frame.items, split == nullptr && in_cockpit() ? 0 : -1);
    }
    if (split == nullptr && cockpit_ && cockpit_up()) {
      cockpit_->add_items(cockpit_input(), clock_, frame.overlay);
    }
    world_->draw(frame);
  }
  const Mat4 view_projection = view * projection;
  if (in_scene()) {
    return;
  }
  if (split == nullptr) {
    hud_->draw(screen, *battle_, view_projection, hud_info(), clock_); // 0x04eaa3
    return;
  }
  BattleHudInfo info;
  info.panels.day = day_;
  info.panels.money = money_;
  info.panels.mission_seconds = clock_;
  split->hud->draw(screen, *battle_, view_projection, info, clock_);
}

} // namespace phantomcrash
