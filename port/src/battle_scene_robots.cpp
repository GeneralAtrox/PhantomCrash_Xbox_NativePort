// Computer robots' entrances (robot state 1, or 5 for a boss: 0x082552,
// 0x088ffa, 0x08928c, 0x0897a2), shielded while they walk out
// (entry_shield.hpp), and their exits (state 6: 0x08224d, 0x089fa3,
// 0x08a161, 0x08a496). A boss's scene takes the camera, the letterbox bars
// and the music; the other robots fight on meanwhile.
#include "phantomcrash/battle_scene.hpp"

#include "phantomcrash/battle_chatter.hpp"

#include <algorithm>

namespace phantomcrash {

bool BattleScene::start_robot_entrance(Battle &battle, const int index) {
  const std::vector<Gate> *list = gates();
  const SceneTrack *scene = track(scene_state::computer);
  if (!tuning_.computer_entrance || list == nullptr || list->empty() || scene == nullptr ||
      index <= 0 || index >= static_cast<int>(battle.robots().size())) {
    return false;
  }
  auto found = std::ranges::find(robots_, index, &RobotEntrance::robot);
  if (found == robots_.end()) {
    found = robots_.insert(robots_.end(), RobotEntrance{.robot = index});
  }
  RobotEntrance &entry = *found;
  entry.state = scene_state::computer;
  entry.gate_flag = 0;
  entry.walking = false;
  entry.delay = tuning_.entry_delay;
  entry.left = scene->frames;
  // 0x0825df outside the story's missions: robot index % gates.
  entry.gate = index % static_cast<int>(list->size());
  entry.origin = (*list)[static_cast<std::size_t>(entry.gate)];
  battle.place(index, entry.origin.position, entry.origin.heading);
  battle.raise_shield(index);
  hold_still(battle, index);
  return true;
}

int BattleScene::robot_state(const int index) const {
  const auto found = std::ranges::find(robots_, index, &RobotEntrance::robot);
  return found == robots_.end() ? scene_state::none : found->state;
}

void BattleScene::walk_robot(Battle &battle, RobotEntrance &entry, const float seconds,
                             const float step) {
  const Combatant &robot = battle.robots()[static_cast<std::size_t>(entry.robot)];
  if (!robot.alive) {
    end_robot(battle, entry);
    return;
  }
  if (entry.state == scene_state::exit) {
    walk_robot_exit(battle, entry, seconds, step);
    return;
  }
  if (cinematic() && cinematic_ != entry.robot) {
    battle.raise_shield(entry.robot);
    hold_still(battle, entry.robot); // it walks out after the boss's scene
    return;
  }
  if (!entry.walking) {
    // 0x088ffa: it waits for the other robots' scenes, then for a busy
    // door, but no longer than its delay.
    // It stays shielded while it waits (0x089032, 0x0892d1).
    const StageDoors *doors = battle.doors();
    battle.raise_shield(entry.robot);
    if (waits_for_others(battle, entry)) {
      hold_still(battle, entry.robot);
      return;
    }
    if (entry.delay > 0.0F && doors != nullptr && !doors->idle(entry.gate)) {
      entry.delay -= step;
      hold_still(battle, entry.robot);
      return;
    }
    start_walking(battle, entry);
  }
  // A boss's scene plays at the battle's pace; other computer robots'
  // play computer_speed times as fast and end when they would stand.
  const bool boss = entry.state == scene_state::boss;
  const float before = entry.left;
  entry.left -= step * (boss ? 1.0F : tuning_.computer_speed);
  open_door(battle, entry.gate, before, entry.left);
  if (entry.left <= (boss ? 0.0F : tuning_.stand_frame)) {
    end_robot(battle, entry);
    return;
  }
  walk_step(battle, entry.robot, entry.state, entry.origin, before, entry.left, seconds);
}

bool BattleScene::waits_for_others(const Battle &battle, const RobotEntrance &entry) const {
  // 0x09199e answers no in the lessons' battle mode 4.
  if (!tuning_.regular_battle) {
    return false;
  }
  if (active()) {
    return true;
  }
  return std::ranges::any_of(robots_, [&](const RobotEntrance &other) {
    const Combatant &robot = battle.robots()[static_cast<std::size_t>(other.robot)];
    return &other != &entry && other.walking && other.state != scene_state::none && robot.rival &&
           robot.boss;
  });
}

void BattleScene::start_walking(Battle &battle, RobotEntrance &entry) {
  entry.walking = true;
  const Combatant &robot = battle.robots()[static_cast<std::size_t>(entry.robot)];
  if (tuning_.regular_battle) {
    const int line = !robot.rival ? chatter_event::entrance
                     : robot.boss ? chatter_event::boss_entrance
                                  : chatter_event::rival_entrance;
    lines_.emplace_back(entry.robot, line);
  }
  // 0x082cff: a boss (on the other side, loadout +0x70 1) takes the camera
  // for its own shot, the first time in a battle only.
  const SceneTrack *scene = track(scene_state::boss);
  if (!robot.rival || !robot.boss || boss_played_ || cinematic() || scene == nullptr) {
    return;
  }
  entry.state = scene_state::boss;
  entry.left = scene->frames;
  cinematic_ = entry.robot;
  letterbox_way_ = 1.0F; // the bars slide in (global +0xef320 2)
  music_ = tuning_.boss_music;
}

void BattleScene::end_robot(Battle &battle, RobotEntrance &entry) {
  // 0x0897a2: it stands and takes up its path.
  entry.state = scene_state::none;
  battle.release(entry.robot);
  if (battle.robots()[static_cast<std::size_t>(entry.robot)].alive) {
    battle.join_path(entry.robot);
  }
  if (cinematic_ == entry.robot) {
    cinematic_ = -1;
    boss_played_ = true; // global +0xef328
    letterbox_way_ = -1.0F;
  }
}

void BattleScene::observe_robot_gates(Battle &battle) {
  // 0x08224d runs for every robot: one that has been out of every gate
  // and walks back into one leaves, while it has armour.
  if (!tuning_.computer_exit) {
    return;
  }
  for (RobotEntrance &entry : robots_) {
    const Combatant &robot = battle.robots()[static_cast<std::size_t>(entry.robot)];
    if (entry.state != scene_state::none || !robot.alive || robot.hp <= 0.0F || robot.frozen) {
      continue;
    }
    const int gate = gate_at(robot);
    if (entry.gate_flag == 0 && gate < 0) {
      entry.gate_flag = 1;
    } else if (entry.gate_flag == 1 && gate >= 0) {
      entry.gate_flag = 2;
      start_robot_exit(battle, entry, gate);
    }
  }
}

void BattleScene::start_robot_exit(Battle &battle, RobotEntrance &entry, const int gate) {
  sign_off(battle, entry.robot);
  const SceneTrack *scene = track(scene_state::exit);
  if (scene == nullptr) {
    return;
  }
  // 0x089fa3: a computer robot's exit has no bars or camera of its own.
  entry.state = scene_state::exit;
  entry.gate = gate;
  entry.origin = (*gates())[static_cast<std::size_t>(gate)];
  entry.heading =
      heading_to(battle.robots()[static_cast<std::size_t>(entry.robot)], entry.origin.position);
  entry.left = scene->frames;
  battle.raise_shield(entry.robot, scene->frames / scene_frames_per_second);
}

void BattleScene::walk_robot_exit(Battle &battle, RobotEntrance &entry, const float seconds,
                                  const float step) {
  entry.left = std::max(entry.left - step, 0.0F);
  if (entry.left > 0.0F) {
    exit_step(battle, entry.robot, entry.heading, entry.origin.position, seconds, step);
    return;
  }
  // 0x08a496: it stands in the gate, shielded, until its door has shut;
  // then 0x047c9e takes it out of the battle.
  hold_still(battle, entry.robot);
  battle.raise_shield(entry.robot, tuning_.gone_shield_frames / scene_frames_per_second);
  if (const StageDoors *doors = battle.doors(); doors == nullptr || doors->idle(entry.gate)) {
    entry.state = scene_state::none;
    battle.withdraw(entry.robot);
  }
}

void BattleScene::hold_still(Battle &battle, const int index) const {
  // A scripted robot stops where it is, its motion held.
  const RobotMover &mover = battle.robots()[static_cast<std::size_t>(index)].mover;
  ScriptedStep still;
  still.heading = mover.heading();
  still.motion = mover.mode() == MoveMode::scripted ? mover.motion() : robot_motion::stand;
  still.loops = mover.motion_loops();
  still.motion_speed = 0.0F;
  battle.script(index, still);
}

} // namespace phantomcrash
