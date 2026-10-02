#pragma once

#include "phantomcrash/json.hpp"
#include "phantomcrash/math3d.hpp"
#include "phantomcrash/skeleton.hpp"
#include "phantomcrash/world_model.hpp"

#include <array>
#include <string>
#include <vector>

namespace phantomcrash {

// Frames of a motion (0x22f4c8, per leg type; 0x08ddb2 copies it to the
// robot): it repeats loop_start..loop_end while it loops and ends at `last`.
// `root_track` (an int in the table, 0 everywhere) names the track whose
// translation moves robots with root motion (0x0910d4).
struct MotionTiming {
  float root_track{};
  float loop_start{};
  float loop_end{};
  float last{};
};

// The robot part tables (robots/catalogue.json; see
// src/phantomcrash_research/robots.py). A part id's type picks the models,
// skeletons, motion banks and paints the part uses; ids are resource ids.
struct RobotCatalogue {
  struct BodyType {
    int model{-1};
    int emblem_model{-1};
    int motions{-1};
    float forearm_turn{}; // radians about x on the forearms (0x0aa499)
  };
  struct LegType {
    int model{-1};
    int motions{-1};
    std::array<std::array<int, 2>, 2> arm_motions{}; // [side][arm class]
    std::vector<int> paints;                         // by colour
    std::vector<MotionTiming> timings;               // by motion id
    float radius{};                                  // collision radius, 0 if unknown
    int shadow_mesh{-1}; // its flat shadow: a mesh of resource 184 (0x242750), -1 for none
  };
  struct ArmType {
    std::array<int, 2> models{-1, -1}; // [side]
    int arm_class{};
  };
  struct ShoulderType {
    std::array<int, 2> models{-1, -1};
  };

  std::vector<int> body_parts; // part id -> type
  std::vector<int> leg_parts;
  std::vector<int> arm_parts;
  std::vector<int> shoulder_parts;
  std::vector<BodyType> body_types;
  std::vector<LegType> leg_types;
  std::vector<ArmType> arm_types;
  std::vector<ShoulderType> shoulder_types;
  std::vector<int> emblems;
};

[[nodiscard]] RobotCatalogue parse_robot_catalogue(const json::JsonValue &document);

// A robot as the save's robot record names it: part ids, -1 for an empty
// arm or shoulder, and its colour (paint) and emblem.
struct RobotParts {
  int body{};
  int legs{};
  std::array<int, 2> arms{-1, -1};      // left, right
  std::array<int, 2> shoulders{-1, -1}; // left, right
  int colour{};
  int emblem{-1};
  bool operator==(const RobotParts &) const = default;
};

// {"body": 0, "legs": 0, "arms": [0, 6], "shoulders": [2, -1], "colour": 23,
//  "emblem": 205}; missing arms and shoulders are empty.
[[nodiscard]] RobotParts parse_robot_parts(const json::JsonValue &value);

// The largest collision radius a robot gets (0x080e41).
inline constexpr float max_robot_radius = 2.0F;

// Everything needed to draw and animate one robot, as asset paths.
struct RobotAssembly {
  int leg_type{-1};
  int body_type{-1};
  std::vector<MotionTiming> timings;
  float radius{};      // the leg type's collision radius, clamped; 0 if unknown
  int shadow_mesh{-1}; // the leg type's flat shadow (RobotCatalogue::LegType)
  std::string body_model;
  std::string legs_model;
  std::array<std::string, 2> arm_models;
  std::array<std::string, 2> shoulder_models;
  std::string paint; // texture every part draws with
  std::string leg_motions;
  std::string body_motions;
  std::array<std::string, 2> arm_motions;
  const SkeletonNode *skeleton{};
  std::array<const SkeletonNode *, 2> arm_skeletons{};
  float forearm_turn{}; // the body type's (RobotCatalogue::BodyType)
};

[[nodiscard]] std::string robot_model_path(int resource);
[[nodiscard]] std::string robot_texture_path(int resource);
[[nodiscard]] std::string motion_bank_path(int resource);

// Throws std::runtime_error when a part id or type is out of range.
[[nodiscard]] RobotAssembly assemble_robot(const RobotCatalogue &catalogue,
                                           const RobotSkeletons &skeletons,
                                           const RobotParts &parts);

// Each drawn node of a part model is skinned rigidly to one bone slot: the
// body's drawing nodes take slots from 0, the legs continue after them;
// hierarchy-only nodes consume no slot (0x0a8f5a). Writes local * bone; a
// node without a posed bone keeps its model placement under `fallback`.
void skin_nodes(const ModelData &model, const PosePart &bones, int first_slot, const Mat4 &fallback,
                std::vector<Mat4> &node_worlds);

} // namespace phantomcrash
