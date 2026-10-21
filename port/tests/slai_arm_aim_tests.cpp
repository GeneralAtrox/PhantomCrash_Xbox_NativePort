// S.L.A.I. arms: the upper-arm raise (0x4e7a90), the weapon look-at
// (0x4e7f60) and the guns' flight from their own records (0x502b10).

#include "test_support.hpp"

#include "phantomcrash/battle_weapons.hpp"
#include "phantomcrash/json.hpp"
#include "phantomcrash/rigid_animation.hpp"
#include "phantomcrash/robot_parts.hpp"
#include "phantomcrash/skeleton.hpp"
#include "phantomcrash/source_arm_aim.hpp"

#include <cmath>
#include <stdexcept>

using namespace phantomcrash;

namespace {

constexpr float tolerance = 1e-4F;

bool same(const Mat4 &a, const Mat4 &b) {
  for (int row = 0; row < 3; ++row) {
    for (int column = 0; column < 3; ++column) {
      if (std::fabs(a.at(row, column) - b.at(row, column)) > tolerance) {
        return false;
      }
    }
  }
  return true;
}

Vec3 row_of(const Mat4 &m, const int row) {
  return {m.at(row, 0), m.at(row, 1), m.at(row, 2)};
}

bool near(const Vec3 a, const Vec3 b) {
  return length(a - b) < tolerance;
}

SourceArmAim pitch_only(const float low, const float high) {
  SourceArmAim aim;
  aim.order = {'x'};
  for (SourceArmAimSide &side : aim.sides) {
    side.node = 0;
    side.pitch_limits = {low, high};
  }
  return aim;
}

std::vector<RigidNode> one_node(const Mat4 &local = {}) {
  RigidNode node;
  node.local = local;
  return {node};
}

} // namespace

TEST_CASE("a partial rotation turns the given share of the way along the shortest arc") {
  CHECK(same(partial_rotation(rotation_y(1.0F), 1.0F), rotation_y(1.0F)));
  CHECK(same(partial_rotation(rotation_y(1.0F), 0.5F), rotation_y(0.5F)));
  CHECK(same(partial_rotation(rotation_x(-0.8F), 0.25F), rotation_x(-0.2F)));
  CHECK(same(partial_rotation(rotation_z(2.0F), 0.0F), Mat4{}));
}

TEST_CASE("the upper arm pitches by the row's base less the target's elevation") {
  // A target 45 degrees above the robot's forward line: 0x4e7c0c's pitch is
  // base - atan2(y, z) of the offset, turned about source x, which the
  // export's mirror carries to the port as the opposite angle.
  const SourceArmAim aim = pitch_only(-1.5F, 1.5F);
  std::vector<RigidNode> nodes = one_node();
  aim_source_arm(aim, 0, {1.0F, Vec3{0.0F, 5.0F, -5.0F}, Vec3{}}, Mat4{}, nodes);
  CHECK(same(nodes[0].local, rotation_x(pi / 4.0F)));

  // Level with the target the arm takes the row's base pitch.
  SourceArmAim based = pitch_only(-1.5F, 1.5F);
  based.sides[1].pitch = 0.3F;
  nodes = one_node();
  aim_source_arm(based, 1, {1.0F, Vec3{0.0F, 0.0F, -20.0F}, Vec3{}}, Mat4{}, nodes);
  CHECK(same(nodes[0].local, rotation_x(-0.3F)));
}

TEST_CASE("the upper arm's pitch stays within the row's limits") {
  const SourceArmAim aim = pitch_only(-0.5F, 0.5F);
  std::vector<RigidNode> nodes = one_node();
  aim_source_arm(aim, 0, {1.0F, Vec3{0.0F, 100.0F, -1.0F}, Vec3{}}, Mat4{}, nodes);
  CHECK(same(nodes[0].local, rotation_x(0.5F)));
  nodes = one_node();
  aim_source_arm(aim, 0, {1.0F, Vec3{0.0F, -100.0F, -1.0F}, Vec3{}}, Mat4{}, nodes);
  CHECK(same(nodes[0].local, rotation_x(-0.5F)));
}

TEST_CASE("the raise weight scales the arm's angles, then turns that share of the way") {
  // 0x4e7cb0 scales the angles by the weight and 0x4e7f04 slerps by it again.
  const SourceArmAim aim = pitch_only(-1.5F, 1.5F);
  std::vector<RigidNode> nodes = one_node();
  aim_source_arm(aim, 0, {0.5F, Vec3{0.0F, 5.0F, -5.0F}, Vec3{}}, Mat4{}, nodes);
  CHECK(same(nodes[0].local, rotation_x(pi / 16.0F)));
  nodes = one_node(rotation_y(0.4F));
  aim_source_arm(aim, 0, {0.0F, Vec3{0.0F, 5.0F, -5.0F}, Vec3{}}, Mat4{}, nodes);
  CHECK(same(nodes[0].local, rotation_y(0.4F))); // lowered: untouched
}

TEST_CASE("the elevation is measured in the body's frame, not the world's") {
  // The body faces world +x (turned -90 degrees about y): a target ahead and
  // above in that frame raises the arm as it would facing -z.
  const SourceArmAim aim = pitch_only(-1.5F, 1.5F);
  const Mat4 body = rotation_y(-pi / 2.0F);
  const Vec3 ahead = transform_point(Vec3{0.0F, 0.0F, -5.0F}, body);
  std::vector<RigidNode> nodes = one_node();
  aim_source_arm(aim, 0, {1.0F, ahead + Vec3{0.0F, 5.0F, 0.0F}, Vec3{}}, body, nodes);
  CHECK(same(nodes[0].local, rotation_x(pi / 4.0F)));
}

TEST_CASE("the yaw and roll turns follow the z mirror's signs") {
  // Level with the target, so only the row's own angles turn the arm: the
  // turn about y is reversed and the one about z kept.
  SourceArmAim aim = pitch_only(-1.5F, 1.5F);
  aim.order = {'y'};
  aim.sides[0].yaw = 0.3F;
  std::vector<RigidNode> nodes = one_node();
  aim_source_arm(aim, 0, {1.0F, Vec3{0.0F, 0.0F, -20.0F}, Vec3{}}, Mat4{}, nodes);
  CHECK(same(nodes[0].local, rotation_y(-0.3F)));
  aim.order = {'z'};
  aim.sides[0].roll = 0.3F;
  nodes = one_node();
  aim_source_arm(aim, 0, {1.0F, Vec3{0.0F, 0.0F, -20.0F}, Vec3{}}, Mat4{}, nodes);
  CHECK(same(nodes[0].local, rotation_z(0.3F)));
}

TEST_CASE("a row without keep_rotation sets the arm's world turn to the aim then the heading") {
  // 0x4e7db4..4e7f14: the node's whole world rotation is cancelled, so a
  // tilted parent and the node's own turn leave no trace; only the row's turn
  // and the body's yaw remain. The node keeps its offset from the parent.
  SourceArmAim aim = pitch_only(-1.5F, 1.5F);
  aim.keep_rotation = false;
  aim.sides[0].node = 1;
  aim.sides[0].pitch = 0.2F;
  RigidNode parent;
  parent.local = rotation_x(0.4F) * rotation_z(-0.6F);
  RigidNode arm;
  arm.parent = 0;
  arm.local = rotation_z(0.3F) * translation({1.0F, 2.0F, 0.5F});
  const Mat4 body = rotation_y(0.5F) * translation({4.0F, 0.0F, -2.0F});
  const Vec3 position = transform_point(Vec3{}, body);
  const Vec3 ahead = transform_point(Vec3{0.0F, 0.0F, -20.0F}, body);
  std::vector<RigidNode> nodes{parent, arm};
  aim_source_arm(aim, 0, {1.0F, ahead, position}, body, nodes);
  const Mat4 world = nodes[1].local * nodes[0].local * body;
  CHECK(same(world, rotation_x(-0.2F) * rotation_y(0.5F)));
  CHECK(near(row_of(nodes[1].local, 3), Vec3{1.0F, 2.0F, 0.5F}));

  // Half raised: the angles are halved, then the world turn goes half way.
  nodes = {parent, arm};
  aim_source_arm(aim, 0, {0.5F, ahead, position}, body, nodes);
  const Mat4 start = rotation_z(0.3F) * nodes[0].local * body;
  const Mat4 goal = rotation_x(-0.1F) * rotation_y(0.5F);
  const Mat4 half = partial_rotation(goal * transposed(start), 0.5F) * start;
  CHECK(same(nodes[1].local * nodes[0].local * body, half));
}

TEST_CASE("a weapon's aim node turns its barrel (+z in port axes) at the target with its y up") {
  // The source's -z barrel arrives at the port's +z through the loader's z
  // mirror: S.L.A.I. muzzles sit at +z of the aim node as the port loads them.
  std::vector<RigidNode> nodes = one_node(rotation_y(1.2F) * rotation_x(0.7F));
  const Vec3 target{10.0F, 0.0F, 0.0F};
  aim_source_weapon(0, 0.0F, {1.0F, target, Vec3{}}, Mat4{}, nodes);
  CHECK(near(row_of(nodes[0].local, 2), Vec3{1.0F, 0.0F, 0.0F})); // +z faces the target
  CHECK(near(row_of(nodes[0].local, 1), Vec3{0.0F, 1.0F, 0.0F}));
  CHECK(near(row_of(nodes[0].local, 0), Vec3{0.0F, 0.0F, -1.0F}));
}

TEST_CASE("a mirrored weapon keeps its mirror while it aims") {
  // The left arm's pair is mirrored (0x1b3590); the aim must not undo it.
  std::vector<RigidNode> nodes = one_node(scaling({-1.0F, 1.0F, 1.0F}));
  const Vec3 target{0.0F, 0.0F, -30.0F};
  aim_source_weapon(0, 0.0F, {1.0F, target, Vec3{}}, Mat4{}, nodes);
  const Mat4 &local = nodes[0].local;
  CHECK(dot(row_of(local, 0), cross(row_of(local, 1), row_of(local, 2))) < 0.0F);
  CHECK(near(row_of(local, 2), Vec3{0.0F, 0.0F, -1.0F}));
}

TEST_CASE("a weapon below a parent faces the target in world space") {
  RigidNode parent;
  parent.local = rotation_y(0.9F) * translation({3.0F, 2.0F, 1.0F});
  RigidNode gun;
  gun.parent = 0;
  std::vector<RigidNode> nodes{parent, gun};
  const Vec3 target{3.0F, 2.0F, -50.0F};
  aim_source_weapon(1, 0.0F, {1.0F, target, Vec3{}}, Mat4{}, nodes);
  const Mat4 world = nodes[1].local * nodes[0].local;
  const Vec3 position{world.at(3, 0), world.at(3, 1), world.at(3, 2)};
  CHECK(near(normalize(row_of(world, 2)), normalize(target - position)));
}

TEST_CASE("arm aim rows parse from the catalogue and reject bad axes") {
  const json::JsonValue value = json::parse(std::string_view{R"({
    "sides": [
      {"node": 4, "pitch": 0.1, "yaw": 0.2, "roll": 0.3, "pitch_limits": [-1, 0.5], "twist": -0.25},
      {"node": 2, "pitch": 0.4, "yaw": 0.5, "roll": 0.6, "pitch_limits": [-0.75, 1]}
    ],
    "order": ["z", "x", "y"],
    "keep_rotation": false
  })"});
  const std::optional<SourceArmAim> aim = parse_source_arm_aim(&value);
  REQUIRE(aim.has_value());
  CHECK(aim->sides[0].node == 4);
  CHECK(aim->sides[1].node == 2);
  CHECK(std::fabs(aim->sides[0].twist + 0.25F) < tolerance);
  CHECK(aim->sides[1].twist == 0.0F);
  CHECK(std::fabs(aim->sides[1].pitch_limits[0] + 0.75F) < tolerance);
  CHECK((aim->order == std::vector<char>{'z', 'x', 'y'}));
  CHECK(!aim->keep_rotation);
  CHECK(!parse_source_arm_aim(nullptr).has_value());

  const json::JsonValue bad = json::parse(std::string_view{R"({
    "sides": [
      {"node": 4, "pitch": 0, "yaw": 0, "roll": 0, "pitch_limits": [0, 0]},
      {"node": 2, "pitch": 0, "yaw": 0, "roll": 0, "pitch_limits": [0, 0]}
    ],
    "order": ["w"]
  })"});
  CHECK_THROWS((void)parse_source_arm_aim(&bad));
}

TEST_CASE("an S.L.A.I. gun's own flight reaches a robot that the template's could not") {
  // S1A01H's record (0x3cf480 + 0x24): a hundred units a tick for one tick.
  WeaponStats stats;
  stats.hud_kind = hud_kind::machine_gun;
  stats.life_frames = 1.0F;
  stats.start_speed = 100.0F;
  stats.top_speed = 100.0F;
  stats.damage = 2385;
  const std::vector<Target> targets{{Vec3{0.0F, 0.0F, -60.0F}, 3.0F, 1, true}};
  const auto hits = [&](const WeaponStats &flight) {
    WeaponTuning gun;
    apply_weapon_stats(gun, flight);
    Armory armory;
    armory.fire(gun, 0, 0, Vec3{}, Vec3{0.0F, 0.0F, -1.0F}, -1, targets);
    const StageCollision open;
    std::vector<WeaponHit> out;
    for (int tick = 0; tick < 30; ++tick) {
      armory.tick(1.0F / 30.0F, targets, open, out);
    }
    return out;
  };
  const std::vector<WeaponHit> own = hits(stats);
  REQUIRE(own.size() == 1);
  CHECK(own.front().victim == 0);
  CHECK(std::fabs(own.front().damage - 2385.0F) < 1e-3F);

  WeaponStats template_flight = stats; // the Phantom Crash template's 3.8
  template_flight.start_speed = template_flight.top_speed = 3.8F;
  CHECK(hits(template_flight).empty());
}
