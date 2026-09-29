#pragma once

#include "phantomcrash/json.hpp"
#include "phantomcrash/math3d.hpp"
#include "phantomcrash/robot.hpp"
#include "phantomcrash/robot_load.hpp"
#include "phantomcrash/training_battle.hpp"

#include <array>
#include <map>
#include <string>
#include <vector>

namespace phantomcrash {

// The frame's last pass before the cockpit (0x0b7900, once a frame ahead of
// view layer 0xb): the scene multiplied by the stage preset's tint, and in
// battles blurred with distance first (0x0b5fb0, dof.xpu).
struct ScreenPass {
  // The environment preset's four ints at +0x24 (A, R, G, B), packed into
  // 0x25b490 by 0x0b20a0 in battle setup (0x044b18); white elsewhere.
  Vec3 tint{1.0F, 1.0F, 1.0F};
  bool depth_of_field{}; // 0x9cb16c: on in battle setup (0x046aa5), off at teardown
  // The stage's focus (the table at 0x203820, 80 on every stage); the
  // blur starts where the depth's top byte passes focus * 0.0005 + 0.95
  // and ramps in over 1/32 of it.
  float focus{80.0F};
  float focus_scale{0.0005F};
  float focus_base{0.95F};
  float ramp{32.0F};
  // The original's battle projection (0x0b7cc0), whose depth the blur
  // reads: the port's depth is turned back into distance and into that.
  float depth_near{0.1F};
  float depth_far{10000.0F};
  // The two blur passes' taps (0x25b6a8, 0x25b768): four corners one
  // texel out, of the 640x480 frame and then of its 320x240 half.
  float blur_texels{1.0F};

  [[nodiscard]] bool active() const noexcept {
    return depth_of_field || tint.x != 1.0F || tint.y != 1.0F || tint.z != 1.0F;
  }
};

// How a stage is lit and fogged. The original sets these per stage in code
// (the per-stage function table at 0x22e928); the values are port choices
// until they are recovered.
struct StageLighting {
  Vec3 ambient{0.45F, 0.45F, 0.45F};
  Vec3 sun_direction{-0.4F, -0.8F, 0.45F}; // the way the light travels
  Vec3 sun_colour{0.75F, 0.75F, 0.72F};
  bool fog{};
  Vec3 fog_colour{0.7F, 0.8F, 0.9F};
  float fog_start{300.0F};
  float fog_end{2500.0F};
  Vec3 clear_colour{0.0F, 0.0F, 0.0F};
  // What the robots' shadow volumes lay over the stage (0x0b6b80, the
  // colour table at 0x2014e8 per stage): 0x70030303 on most stages.
  std::array<float, 4> shadow_colour{3 / 255.0F, 3 / 255.0F, 3 / 255.0F, 0x70 / 255.0F};
  ScreenPass screen;
};

// One battle stage of the stage table at 0x2212ac (data/world/stages.json).
struct StageDefinition {
  std::string name;
  std::string model;                  // asset path of its glTF, empty until converted
  std::string collision;              // asset path of its converted collision, or empty
  std::string enemy_path;             // asset path of its drones' path (enepath/), or empty
  std::vector<std::string> sky_nodes; // drawn around the camera, behind everything
  StageLighting lighting;
  int scene_stage{-1};    // its number in the original's tables (0x2212ac), -1 for the script's
  std::string door_model; // the effect model of its gate doors, empty for the script's
};

[[nodiscard]] std::map<std::string, StageDefinition> parse_stages(const json::JsonValue &document);

// The scenes' gates, doors and cameras for the stage the battle is on.
void apply_stage(SceneTuning &scenes, const StageDefinition &stage);

// The tutorial battle (scene 42), opened by one of Pepper's lessons. The
// guide's lesson (0x2928c) picks the save's mode at +0x10f10: FreePlay 3,
// basic controls 0, attack methods 1, Optic Camouflage 2.
struct TrainingLesson {
  std::string title;
  int mode{};
  // Split screen (split_screen.hpp): the screens, each following robots
  // 0, 1, ... A port test of the versus mode's split HUD; 1 in the
  // original's lessons.
  int screens{1};
};

struct TrainingScript {
  std::string stage;
  std::string music;
  std::string return_to;
  std::map<std::string, TrainingLesson> lessons; // screen name -> lesson
  std::map<std::string, RobotParts> robots;      // "player", "drone", ...
  TrainingBattle battle;
};

[[nodiscard]] TrainingScript parse_training(const json::JsonValue &document);

// What the training robot `name` carries: its body and legs (script.robots)
// at the robot's levels, its chip and the parts its weapons name (battle.settings).
[[nodiscard]] RobotLoadout training_loadout(const TrainingScript &script, const std::string &name,
                                            const TrainingRobot &robot);

} // namespace phantomcrash
