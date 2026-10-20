// The opt-in mixed-part body fit adapter (body_fit.hpp).
#include "test_support.hpp"
#include "phantomcrash/body_fit.hpp"
#include "phantomcrash/data_roots.hpp"
#include "phantomcrash/robot.hpp"
#include "phantomcrash/weapon_mounts.hpp"
#include <cmath>

using namespace phantomcrash;

namespace {
bool near(const Vec3 a, const Vec3 b, const float tolerance = 1e-4F) {
  return length(a - b) <= tolerance;
}
Vec3 origin(const Mat4 &m) {
  return {m.at(3, 0), m.at(3, 1), m.at(3, 2)};
}
Vec3 axis(const Mat4 &m, const int index) {
  return {m.at(index, 0), m.at(index, 1), m.at(index, 2)};
}

// A leg control (bone 1) carrying the mount (bone 0, the node that turns
// with the aim), and a body node one unit out from it (bone 2).
SkeletonNode mounted_skeleton() {
  return parse_skeleton_node(json::parse(R"({"type": 0, "track": -1, "bone": -1,
    "translation": [0, 0, 0], "rotation": [0, 0, 0], "children": [
      {"type": 0, "track": -1, "bone": 1, "flags": 1, "translation": [0, 2, 0],
       "rotation": [0, 0, 0.3], "children": [
        {"type": 0, "track": -1, "bone": 0, "flags": 4, "translation": [0, 0, 0],
         "rotation": [0, 0, 0], "children": [
          {"type": 0, "track": -1, "bone": 2, "flags": 1, "translation": [1, 0.5, 0],
           "rotation": [0, 0, 0]}]}]}]})"));
}

Pose posed(const SkeletonNode &skeleton) {
  PoseInput input;
  input.root = rotation_y(0.7F) * translation({17, 3, -21});
  input.aim_yaw = 0.25F;
  Pose pose;
  build_pose(skeleton, input, pose);
  return pose;
}
} // namespace

TEST_CASE("body fits parse and choose the most specific enabled entry") {
  const auto table = parse_body_fits(json::parse(R"({"enabled": true, "fits": [
    {"body_model": 12002, "scale": 1.1},
    {"legs_model": 14102, "scale": 1.2},
    {"legs_model": 14102, "body_model": 12002, "scale": 1.3, "offset": [0, 0.5, 0]},
    {"legs_model": 14102, "body_model": 12002, "scale": 9}]})"));
  REQUIRE(table.fits.size() == 4);
  CHECK(find_body_fit(table, 14102, 12002)->scale == 1.3F);
  CHECK(near(find_body_fit(table, 14102, 12002)->offset, {0, 0.5F, 0}));
  CHECK(find_body_fit(table, 14102, 38)->scale == 1.2F);
  CHECK(find_body_fit(table, 39, 12002)->scale == 1.1F);
  CHECK(find_body_fit(table, 39, 38) == nullptr);
  auto disabled = table;
  disabled.enabled = false;
  CHECK(find_body_fit(disabled, 14102, 12002) == nullptr);
  CHECK(is_identity_fit(BodyFit{}));
  CHECK_THROWS(parse_body_fits(json::parse(R"({"fits": [{"scale": 0}]})")));
  CHECK_THROWS(parse_body_fits(json::parse(R"({"fits": [{"offset": [0, 1]}]})")));
}

TEST_CASE("the shipped body fit table keeps the original join") {
  // The original attaches every body at unit scale with no offset
  // (0x2689a0..9d4), so the shipped adapter table is off.
  const auto roots = DataRoots::shipped(PHANTOMCRASH_SOURCE_ROOT);
  const auto table = parse_body_fits(roots.document("world/body_fit.json"));
  CHECK(!table.enabled);
  for (const BodyFit &fit : table.fits) {
    CHECK(fit.legs_model >= 0 || fit.body_model >= 0);
  }
}

TEST_CASE("a body fit scales and moves the body about its mount and leaves the legs") {
  const SkeletonNode plain = mounted_skeleton();
  BodyFit fit;
  fit.scale = 1.5F;
  fit.offset = {0, 0.4F, -0.2F};
  const auto fitted = fit_body_mount(plain, body_fit_transform(fit));
  REQUIRE(fitted != nullptr);
  const Pose before = posed(plain);
  const Pose after = posed(*fitted);
  for (int bone = 0; bone < 3; ++bone) {
    REQUIRE(before[0].set[static_cast<std::size_t>(bone)]);
    REQUIRE(after[0].set[static_cast<std::size_t>(bone)]);
  }
  const Mat4 &leg_before = before[0].bones[1];
  const Mat4 &leg_after = after[0].bones[1];
  for (std::size_t i = 0; i < leg_before.m.size(); ++i) {
    CHECK(std::fabs(leg_before.m[i] - leg_after.m[i]) < 1e-5F);
  }
  // The mount moves by the offset in its own (aimed) space.
  const Mat4 &mount = before[0].bones[0];
  CHECK(near(origin(after[0].bones[0]), transform_point(fit.offset, mount)));
  REQUIRE(after[0].mount.has_value());
  CHECK(near(origin(*after[0].mount), origin(after[0].bones[0])));
  for (int i = 0; i < 3; ++i) {
    CHECK(std::fabs(length(axis(after[0].bones[0], i)) - 1.5F) < 1e-4F);
    CHECK(near(axis(after[0].bones[0], i), axis(mount, i) * 1.5F));
  }
  // Below the mount, the body grows about the mount.
  const Vec3 reach = origin(before[0].bones[2]) - origin(mount);
  CHECK(near(origin(after[0].bones[2]) - origin(after[0].bones[0]), reach * 1.5F));
  CHECK(fit_body_mount(SkeletonNode{}, body_fit_transform(fit)) == nullptr);
}

TEST_CASE("pose blending and aiming keep a fitted body's uniform scale") {
  const Mat4 a = scaling({1.5F, 1.5F, 1.5F}) * rotation_y(0.2F) * translation({1, 2, 3});
  const Mat4 b = scaling({1.5F, 1.5F, 1.5F}) * rotation_y(0.8F) * translation({3, 2, 1});
  const Mat4 mid = blend_pose_transform(a, b, 0.5F);
  const Mat4 expected = scaling({1.5F, 1.5F, 1.5F}) * rotation_y(0.5F) * translation({2, 2, 2});
  for (std::size_t i = 0; i < mid.m.size(); ++i) {
    CHECK(std::fabs(mid.m[i] - expected.m[i]) < 1e-4F);
  }
  const Mat4 aimed = aim_pose_transform(a, {10, 2, 3}, {0, 1, 0});
  for (int i = 0; i < 3; ++i) {
    CHECK(std::fabs(length(axis(aimed, i)) - 1.5F) < 1e-4F);
  }
  // Unscaled matrices take the original path unchanged.
  const Mat4 plain = blend_pose_transform(rotation_y(0.2F), rotation_y(0.8F), 0.5F);
  CHECK(std::fabs(length(axis(plain, 0)) - 1) < 1e-5F);
}

TEST_CASE("the catalogue's enabled body fit reaches the assembled skeleton") {
  auto catalogue = parse_robot_catalogue(json::parse(R"({
    "parts": {"body": [0], "legs": [0], "arm": [], "shoulder": []},
    "arm_types": [], "shoulder_types": [], "emblems": [],
    "body_types": [{"model": 38}], "leg_types": [{"model": 39}]})"));
  RobotSkeletons skeletons;
  skeletons.legs.emplace_back(mounted_skeleton());
  const RobotParts parts;
  CHECK(assemble_robot(catalogue, skeletons, parts).skeleton == &*skeletons.legs[0]);
  catalogue.body_fits = parse_body_fits(json::parse(R"({"enabled": true, "fits": [
    {"legs_model": 39, "body_model": 38, "scale": 1.25}]})"));
  const RobotAssembly robot = assemble_robot(catalogue, skeletons, parts);
  REQUIRE(robot.fitted != nullptr);
  CHECK(robot.skeleton == robot.fitted.get());
  CHECK(robot.grafted == nullptr);
  const Pose pose = posed(*robot.skeleton);
  CHECK(std::fabs(length(axis(pose[0].bones[0], 0)) - 1.25F) < 1e-4F);
  catalogue.body_fits.fits[0].scale = 1;
  CHECK(assemble_robot(catalogue, skeletons, parts).fitted == nullptr);
}
