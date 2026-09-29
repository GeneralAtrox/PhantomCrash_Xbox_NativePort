#pragma once

#include "phantomcrash/battle.hpp"
#include "phantomcrash/battle_camera.hpp"
#include "phantomcrash/battle_scene.hpp"
#include "phantomcrash/chase_camera.hpp"
#include "phantomcrash/glyph_font.hpp"
#include "phantomcrash/input.hpp"
#include "phantomcrash/json.hpp"
#include "phantomcrash/entry_shield.hpp"
#include "phantomcrash/optical_camouflage.hpp"
#include "phantomcrash/robot_shadow.hpp"
#include "phantomcrash/supply_drop.hpp"
#include "phantomcrash/story_event.hpp"
#include "phantomcrash/tutorial.hpp"

#include <array>
#include <map>
#include <string>
#include <vector>

namespace phantomcrash {

// A robot of the tutorial battle: its weapon per WeaponSlot (names of the
// battle's weapons, empty for none), its armour and its AI chip (an index
// into tables/parts.json "chips", -1 for none) at a level.
struct TrainingRobot {
  std::array<std::string, weapon_slots> weapons;
  float hp{100.0F}; // without the part tables, or when armour_from_parts is off
  int chip{-1};
  int chip_level{};
  int body_level{}; // the body's and legs' (script.robots) levels
  int legs_level{};
  int pilot{-1}; // a pilot record (0x1c9920); its tag labels the marker
  bool boss{};   // a boss (loadout +0x70 1): a drone enters with the boss's scene
};

// The battle buttons (the lessons' own texts name them: triggers fire the
// arms, B and Y the shoulders, X Optic Camouflage, white and black dodge).
struct BattleButtons {
  std::array<Buttons, weapon_slots> fire{button::right_trigger, button::left_trigger, button::b,
                                         button::y};
  Buttons camouflage{button::x};
  Buttons jump{button::a};
  Buttons dash{button::left_thumb};
  Buttons reset_twist{button::right_thumb};
  Buttons dodge_left{button::white};
  Buttons dodge_right{button::black};
  Buttons exit{button::back};
  // The camera's views (0x0774ad): the D-pad.
  Buttons view_first_person{button::pad_up};
  Buttons view_behind{button::pad_down};
  Buttons view_next{button::pad_right};
  Buttons view_previous{button::pad_left};
  // The buttons' names on the HUD, per WeaponSlot.
  std::array<std::string, weapon_slots> fire_labels{"RT", "LT", "B", "Y"};
};

// The "battle" section of data/world/training.json.
struct TrainingBattle {
  BattleSettings settings;
  ChaseCameraSettings camera;
  CameraViewSettings views;
  OpticalCamouflage optical;
  EntryShieldLook shield_look;
  RobotShadowLook shadows;
  // FreePlay's supply drops; the stage's routes come from the scenes'
  // tables (tables/battle_scenes.json).
  SupplySettings supply;
  BattleButtons buttons;
  std::map<int, std::vector<LessonStep>> courses; // by lesson mode
  std::string lessons{"tables/lessons.json"};     // Pepper's messages (converted)
  // The battle chatter (converted, battle_chatter.hpp), the sound a line
  // opens with (0x049552 plays battle bank entry 45) and the rate its
  // scripts' frames run at: the battle's logic rate (port reading).
  std::string chatter{"tables/chatter.json"};
  std::string chatter_sound{"chatter"};
  double chatter_frame_rate{30.0};
  std::string enemy_path{"enepath/choco_es_01_enepath.json"};
  TrainingRobot player;
  Vec3 player_start{0.0F, 0.0F, 40.0F};
  float player_heading{};
  TrainingRobot drone;
  int drones{4};
  // Walk and dash at the pace the leg motions stride (the original moves
  // robots by root motion); false keeps the "movement" speeds.
  bool stride_speeds{true};
  // Armour from the robots' body and legs records (0x076114); false keeps
  // the robots' "hp".
  bool armour_from_parts{true};
  // FreePlay's entrance and exit (battle_scene.hpp).
  SceneTuning scenes;
};

[[nodiscard]] TrainingBattle parse_training_battle(const json::JsonValue &value);

// Pepper's in-battle messages by lesson state: [0] the first, [1] the
// second, each empty when the state has none (tables/lessons.json). The
// Standard control type has its own wording of some ("standard", the table
// at 0x243220; 0x091f8f picks it by the robot's control flag bit 0).
using LessonMessages = std::map<int, std::array<std::vector<EventCommand>, 2>>;
[[nodiscard]] LessonMessages parse_lesson_messages(const json::JsonValue &document,
                                                   const GlyphFont &font, bool standard_controls);

// The player's stick and buttons as robot controls. `step` (may be null)
// limits the walk and the turn direction as the basic lesson does.
[[nodiscard]] RobotControl battle_control(const InputFrame &input, const BattleButtons &buttons,
                                          const LessonStep *step);
[[nodiscard]] FireControl battle_fire(const InputFrame &input, const BattleButtons &buttons);

// What `player` did over the tick of `seconds` that started at
// `heading_before`, for the lesson goals.
[[nodiscard]] LessonSample lesson_sample(const Combatant &player, float heading_before,
                                         float seconds);

} // namespace phantomcrash
