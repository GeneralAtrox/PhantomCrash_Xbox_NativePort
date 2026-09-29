#include "test_support.hpp"

#include "phantomcrash/settings.hpp"
#include "phantomcrash/training_battle.hpp"

#include <algorithm>
#include <cmath>

using namespace phantomcrash;

TEST_CASE("desktop bindings and mouse preferences survive a settings save") {
  Settings settings;
  bind_host_key(settings.host_controls, "left_trigger", "W");
  bind_host_pad(settings.host_controls, "left_trigger", "a");
  settings.host_controls.invert_mouse_y = true;
  settings.host_controls.mouse_degrees_per_pixel = 0.32F;
  const Settings restored = settings_from_json(settings_to_json(settings));
  CHECK(restored.host_controls == settings.host_controls);
  const auto &bindings = restored.host_controls.bindings;
  const auto forward = std::ranges::find(bindings, "up", &HostBinding::action);
  CHECK(std::ranges::find(forward->keys, "W") == forward->keys.end());
  const auto jump = std::ranges::find(bindings, "a", &HostBinding::action);
  CHECK(jump->pad.empty());
  CHECK(settings_from_json(json::parse("{}")).host_controls == HostControls{});
}

TEST_CASE("left and right mouse clicks fire only their corresponding arm mounts") {
  InputFrame input;
  input.mouse_fire_left = true;
  const FireControl left = battle_fire(input, BattleButtons{});
  CHECK(left.fire[static_cast<std::size_t>(WeaponSlot::left_arm)]);
  CHECK(!left.fire[static_cast<std::size_t>(WeaponSlot::right_arm)]);
  CHECK(!left.fire[static_cast<std::size_t>(WeaponSlot::left_shoulder)]);
  CHECK(!left.fire[static_cast<std::size_t>(WeaponSlot::right_shoulder)]);
  input.mouse_fire_left = false;
  input.mouse_fire_right = true;
  const FireControl right = battle_fire(input, BattleButtons{});
  CHECK(right.fire[static_cast<std::size_t>(WeaponSlot::right_arm)]);
  CHECK(!right.fire[static_cast<std::size_t>(WeaponSlot::left_arm)]);
}

TEST_CASE("mouse aim is a displacement and does not repeat after it is consumed") {
  MovementTuning tuning;
  RobotMover mover(tuning);
  mover.place({}, 0.0F);
  InputFrame input;
  input.mouse_aim = {0.2F, 0.1F};
  mover.tick(battle_control(input, BattleButtons{}, nullptr), 1.0F / 60.0F);
  CHECK(std::abs(mover.heading() - 0.2F) < 0.0001F);
  CHECK(std::abs(mover.pitch() - 0.1F) < 0.0001F);
  mover.tick(RobotControl{}, 1.0F / 30.0F);
  CHECK(std::abs(mover.heading() - 0.2F) < 0.0001F);
  CHECK(std::abs(mover.pitch() - 0.1F) < 0.0001F);
}
