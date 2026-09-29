// Robots: motion channels, skeleton poses with their leg IK, and the part
// catalogue.

#include "test_support.hpp"

#include <cmath>

#include "phantomcrash/json.hpp"
#include "phantomcrash/motion.hpp"
#include "phantomcrash/robot.hpp"
#include "phantomcrash/skeleton.hpp"

using namespace phantomcrash;

namespace {

bool near(const float a, const float b, const float tolerance = 1e-4F) {
  return std::fabs(a - b) <= tolerance;
}

bool near(const Vec3 a, const Vec3 b, const float tolerance = 1e-3F) {
  return near(a.x, b.x, tolerance) && near(a.y, b.y, tolerance) && near(a.z, b.z, tolerance);
}

Vec3 origin(const Mat4 &m) {
  return {m.at(3, 0), m.at(3, 1), m.at(3, 2)};
}

MotionChannel channel(const ChannelKind kind, std::vector<float> keys, std::vector<float> values) {
  MotionChannel c;
  c.kind = kind;
  c.keys = std::move(keys);
  c.values = std::move(values);
  return c;
}

} // namespace

TEST_CASE("motion channels interpolate like the original's reader") {
  CHECK(near(channel(ChannelKind::constant, {}, {2.5F}).sample(9.0F), 2.5F));
  const MotionChannel linear = channel(ChannelKind::linear, {10, 20}, {0, 10, 30});
  CHECK(near(linear.sample(5.0F), 5.0F));
  CHECK(near(linear.sample(15.0F), 20.0F));
  CHECK(near(linear.sample(99.0F), 30.0F)); // holds past the last key
  const MotionChannel step = channel(ChannelKind::step, {10}, {1, 2});
  CHECK(near(step.sample(9.0F), 1.0F));
  CHECK(near(step.sample(10.5F), 2.0F));
  const MotionChannel smooth = channel(ChannelKind::smooth, {10}, {0, 1});
  CHECK(near(smooth.sample(5.0F), 0.5F));
  CHECK(near(smooth.sample(2.5F), 3 * 0.0625F - 2 * 0.015625F));
  // A full Hermite key with no tangents is the smoothstep; tangents add the
  // (f - 1) * elapsed * ((f - 1) m0 + f m1) term.
  const MotionChannel flat = channel(ChannelKind::hermite, {10}, {0, 0, 1, 0});
  CHECK(near(flat.sample(5.0F), 0.5F));
  const MotionChannel sloped = channel(ChannelKind::hermite, {10}, {0, 1, 1, 0});
  CHECK(near(sloped.sample(5.0F), 0.5F + (-0.5F) * 5.0F * (-0.5F)));
}

TEST_CASE("motion banks parse and sample their tracks") {
  const auto bank = parse_motion_bank(json::parse(R"({"track_count": 1, "motions": [null,
    {"frames": 10, "tracks": [{"flags": 227, "translation": [
      {"kind": "constant", "values": [1]},
      {"kind": "linear", "keys": [10], "values": [0, 4]},
      {"kind": "constant", "values": [0]}]}]}]})"));
  CHECK(bank.motion(0) == nullptr);
  const Motion *motion = bank.motion(1);
  CHECK(motion != nullptr);
  const TrackSample sample = motion->sample(0, 5.0F);
  CHECK(!sample.rotation && sample.translation);
  CHECK(near(*sample.translation, {1.0F, 2.0F, 0.0F}));
  CHECK_THROWS(parse_motion_bank(json::parse(R"({"track_count": 2, "motions": [
    {"frames": 1, "tracks": []}]})")));
}

TEST_CASE("a static skeleton places its bones with the game's axis conventions") {
  // Root at (0, 2, 0); a child moved 1 along stored -Z (game +Z) and turned
  // a quarter about Y (stored -pi/2: the evaluator negates X and Y angles).
  const SkeletonNode root = parse_skeleton_node(json::parse(R"({"type": 0, "track": -1,
    "translation": [0, 2, 0], "rotation": [0, 0, 0], "bone": 0, "children": [
      {"type": 0, "track": -1, "translation": [0, 0, -1], "rotation": [0, -1.5707963, 0],
       "bone": 1}]})"));
  PoseInput input;
  Pose pose;
  build_pose(root, input, pose);
  CHECK(pose[0].set[0] && pose[0].set[1] && !pose[0].set[2]);
  CHECK(near(origin(pose[0].bones[0]), {0.0F, 2.0F, 0.0F}));
  CHECK(near(origin(pose[0].bones[1]), {0.0F, 2.0F, 1.0F}));
  // Rotation Y(+pi/2), D3DX: the child's X axis points along -Z.
  CHECK(near(transform_point({1, 0, 0}, pose[0].bones[1]), {0.0F, 2.0F, 0.0F}));
}

TEST_CASE("a forearm turns about X by its body type's turn") {
  // Flag 0x10 marks a forearm (0x0aa499); a Photon body turns it by -pi/2.
  const SkeletonNode root = parse_skeleton_node(json::parse(R"({"type": 0, "track": -1,
    "translation": [0, 0, 0], "rotation": [0, 0, 0], "bone": 0, "flags": 16})"));
  PoseInput input;
  Pose pose;
  build_pose(root, input, pose);
  CHECK(near(transform_point({0, 1, 0}, pose[0].bones[0]), {0.0F, 1.0F, 0.0F}));
  input.forearm_turn = -1.5707963F;
  build_pose(root, input, pose);
  CHECK(near(transform_point({0, 1, 0}, pose[0].bones[0]), {0.0F, 0.0F, -1.0F}));
}

TEST_CASE("leg chains aim one bone and bend two bones to reach their track") {
  // Hip chain at the origin; one bone of 1 aims at track 0, then two bones
  // of 1 reach track 1.
  const SkeletonNode root = parse_skeleton_node(json::parse(R"({"type": 2, "track": -1,
    "translation": [0, 0, 0], "rotation": [0, 0, 0], "children": [
      {"type": 3, "translation": [0, 0, 0], "rotation": [0, 0, 0], "bone": 0, "children": [
        {"type": 4, "track": 0, "translation": [1, 0, 0], "rotation": [0, 0, 0], "children": [
          {"type": 2, "translation": [0, 0, 0], "rotation": [0, 0, 0], "children": [
            {"type": 3, "translation": [0, 0, 0], "rotation": [0, 0, -0.5], "bone": 1,
             "children": [
              {"type": 3, "translation": [1, 0, 0], "rotation": [0, 0, 1.0], "bone": 2,
               "children": [
                {"type": 4, "track": 1, "translation": [1, 0, 0], "rotation": [0, 0, 0],
                 "bone": 3}]}]}]}]}]}]})"));
  // Targets as stored: the evaluator flips Y of animated translations.
  const auto bank = parse_motion_bank(json::parse(R"({"track_count": 2, "motions": [
    {"frames": 0, "tracks": [
      {"flags": 227, "translation": [{"kind": "constant", "values": [0]},
        {"kind": "constant", "values": [1]}, {"kind": "constant", "values": [0]}]},
      {"flags": 227, "translation": [{"kind": "constant", "values": [1.2]},
        {"kind": "constant", "values": [1]}, {"kind": "constant", "values": [0]}]}]}]})"));
  PoseInput input;
  input.legs.motion = bank.motion(0);
  Pose pose;
  build_pose(root, input, pose);
  // The thigh points straight down at (0, -1, 0).
  CHECK(near(transform_point({1, 0, 0}, pose[0].bones[0]), {0.0F, -1.0F, 0.0F}));
  // The shin's end sits on the second target, (1.2, -1, 0).
  CHECK(pose[0].set[3]);
  CHECK(near(origin(pose[0].bones[3]), {1.2F, -1.0F, 0.0F}));
  CHECK(near(length(origin(pose[0].bones[2]) - origin(pose[0].bones[1])), 1.0F));
}

TEST_CASE("the catalogue assembles a robot's models, motions and paint") {
  const auto catalogue = parse_robot_catalogue(json::parse(R"({
    "parts": {"body": [0], "legs": [0], "arm": [1], "shoulder": [0]},
    "body_types": [{"model": 38, "emblem_model": 177, "motions": 1722}],
    "leg_types": [{"model": 39, "motions": 1727, "arm_motions": [[1723, 1724], [1725, 1726]],
                   "paints": [383, 384]}],
    "arm_types": [{"models": [9, 0], "class": 0}, {"models": [10, 1], "class": 1}],
    "shoulder_types": [{"models": [28, 18]}],
    "emblems": [1349]})"));
  RobotSkeletons skeletons;
  skeletons.legs.emplace_back(SkeletonNode{});
  skeletons.arms[0].resize(2);
  skeletons.arms[1].resize(2);
  RobotParts parts;
  parts.arms = {0, 0};
  parts.shoulders = {-1, 0};
  parts.colour = 1;
  const RobotAssembly robot = assemble_robot(catalogue, skeletons, parts);
  CHECK(robot.body_model == "models/binlink/38.gltf");
  CHECK(robot.legs_model == "models/binlink/39.gltf");
  CHECK(robot.arm_models[0] == "models/binlink/10.gltf");
  CHECK(robot.arm_models[1] == "models/binlink/1.gltf");
  CHECK(robot.arm_motions[0] == "motions/1724.json"); // arm class 1
  CHECK(robot.shoulder_models[0].empty() && robot.shoulder_models[1] == "models/binlink/18.gltf");
  CHECK(robot.paint == "textures/binlink/384.png");
  CHECK(robot.skeleton != nullptr);
  parts.legs = 3;
  CHECK_THROWS(assemble_robot(catalogue, skeletons, parts));
}

TEST_CASE("drawn nodes skin rigidly to consecutive bone slots") {
  ModelData model;
  model.nodes.resize(2);
  model.nodes[0].local = translation({1, 0, 0});
  model.nodes[1].local = translation({0, 1, 0});
  model.nodes[1].world = translation({5, 5, 5});
  PosePart bones;
  bones.bones[4] = translation({0, 0, 10});
  bones.set[4] = true;
  std::vector<Mat4> worlds;
  skin_nodes(model, bones, 4, Mat4{}, worlds);
  CHECK(worlds.size() == 2);
  CHECK(near(origin(worlds[0]), {1.0F, 0.0F, 10.0F}));
  CHECK(near(origin(worlds[1]), {5.0F, 5.0F, 5.0F})); // slot 5 is not posed
}
