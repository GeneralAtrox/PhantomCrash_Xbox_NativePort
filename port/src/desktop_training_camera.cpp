// The training battle's camera: the chase view, the other views the D-pad
// picks (battle_camera.hpp) and the scenes' scripted shots over both.
#include "desktop_training.hpp"

#include <algorithm>
#include <cmath>
#include <optional>

namespace phantomcrash {
namespace {

// The share of the first-person view past which the camera is in the
// cockpit and the player's robot is hidden (port choice).
constexpr float inside_share = 0.9F;
// The cockpit comes up over the second half of a change to first person
// and goes over the first half of a change away (0x04eaa3).
constexpr float cockpit_share = 0.5F;

} // namespace

CameraSubject TrainingScreen::camera_subject() const {
  const Combatant &player = battle_->player();
  CameraSubject subject;
  subject.position = player.mover.position();
  subject.yaw = player.mover.aim_heading();
  subject.pitch = player.mover.pitch();
  subject.height = player.radius;
  const std::optional<Vec3> torso = view_ ? view_->torso(0) : std::nullopt;
  // Without a posed robot, the torso is taken as two leg radii up.
  subject.torso = torso.value_or(subject.position + Vec3{0.0F, player.radius * 2.0F, 0.0F});
  subject.legs_model = view_ ? view_->legs_model(0) : 0;
  return subject;
}

bool TrainingScreen::in_cockpit() const {
  return !in_scene() && views_.first_person_share() >= inside_share;
}

bool TrainingScreen::cockpit_up() const {
  return !in_scene() && views_.first_person_share() >= cockpit_share;
}

CockpitInput TrainingScreen::cockpit_input() const {
  const Combatant &player = battle_->player();
  const RobotMover &mover = player.mover;
  const Vec3 velocity = mover.velocity();
  CockpitInput input;
  input.vertical_speed = velocity.y;
  input.gravity = mover.tuning().gravity;
  const float dash = mover.tuning().dash_speed;
  input.speed_ratio = dash > 0.0F ? std::hypot(velocity.x, velocity.z) / dash : 0.0F;
  input.armour = player.max_hp > 0.0F ? player.hp / player.max_hp : 0.0F;
  input.hit = player.hit_flash > 0.0F;
  input.camouflage = player.camouflage;
  input.camouflaged = player.camouflaged;
  input.sway = wrap_angle(mover.aim_heading() - views_.yaw());
  // HUD state +0x97c: 0 in place, 1 out of view at the blend's middle.
  input.slide = std::clamp(2.0F * (1.0F - views_.first_person_share()), 0.0F, 1.0F);
  return input;
}

void TrainingScreen::tick_camera(const InputFrame &input, const float seconds) {
  // 0x0774ad: the views change only while the robot is free to move.
  const BattleButtons &buttons = script_.battle.buttons;
  if (!in_scene() && battle_->player().alive) {
    if (input.any_pressed(buttons.view_first_person)) {
      static_cast<void>(views_.change(ViewChange::first_person));
    } else if (input.any_pressed(buttons.view_behind)) {
      static_cast<void>(views_.change(ViewChange::behind));
    } else if (input.any_pressed(buttons.view_next)) {
      static_cast<void>(views_.change(ViewChange::next));
    } else if (input.any_pressed(buttons.view_previous)) {
      static_cast<void>(views_.change(ViewChange::previous));
    }
  }
  const RobotMover &mover = battle_->player().mover;
  camera_.tick(mover.position(), mover.aim_heading(), mover.pitch(),
               battle_->settings().arena.half_size, seconds, snap_camera_);
  // The eye stays in front of a shut door (a port choice).
  const StageDoors *doors = battle_->doors();
  if (const auto hit =
          doors ? doors->segment_hit(camera_.chase().target, camera_.chase().eye) : std::nullopt) {
    camera_.block_eye(camera_.chase().target, *hit);
  }
  // A fixed camera needs a clear line to the robot (0x053db4, 0x06fd32).
  const SightTest sees = [this](const Vec3 from, const Vec3 to) {
    const StageDoors *shut = battle_->doors();
    return !battle_->stage().segment_hit(from, to) && !(shut && shut->segment_hit(from, to));
  };
  views_.tick(camera_subject(), camera_.chase(), seconds, sees, snap_camera_);
  snap_camera_ = false;
  if (cockpit_) {
    cockpit_->tick(cockpit_input(), seconds);
  }
  std::optional<CameraShot> shot = scene_ ? scene_->scene().shot(camera_.chase()) : std::nullopt;
  camera_.set_shot(shot ? shot : std::optional<CameraShot>(views_.shot()));
}

} // namespace phantomcrash
