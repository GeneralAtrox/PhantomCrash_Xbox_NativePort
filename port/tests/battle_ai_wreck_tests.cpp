// Port addition: computer robots shoot wrecks apart (battle_ai_wrecks.cpp).

#include "test_support.hpp"

#include "phantomcrash/battle.hpp"
#include "phantomcrash/data_roots.hpp"
#include "phantomcrash/json.hpp"

using namespace phantomcrash;

namespace {

constexpr float tick = 1.0F / 60.0F;

// An axis-aligned box's faces, turned outward.
std::vector<CollisionTriangle> box_faces(const Vec3 centre, const float half) {
  std::vector<CollisionTriangle> faces;
  for (int axis = 0; axis < 3; ++axis) {
    for (const float side : {-1.0F, 1.0F}) {
      const int u = (axis + 1) % 3, v = (axis + 2) % 3;
      std::array<Vec3, 4> corners{};
      const std::array<std::array<float, 2>, 4> square{{{-1, -1}, {1, -1}, {1, 1}, {-1, 1}}};
      for (std::size_t corner = 0; corner < 4; ++corner) {
        std::array<float, 3> point{centre.x, centre.y, centre.z};
        point[static_cast<std::size_t>(axis)] += side * half;
        point[static_cast<std::size_t>(u)] += square[corner][0] * half;
        point[static_cast<std::size_t>(v)] += square[corner][1] * half;
        corners[corner] = {point[0], point[1], point[2]};
      }
      std::array<float, 3> normal{};
      normal[static_cast<std::size_t>(axis)] = side;
      CollisionTriangle first, second;
      first.points = {corners[0], corners[1], corners[2]};
      second.points = {corners[0], corners[2], corners[3]};
      first.normal = second.normal = {normal[0], normal[1], normal[2]};
      faces.push_back(first);
      faces.push_back(second);
    }
  }
  return faces;
}

StageCollision box(const Vec3 centre, const float half) {
  return StageCollision(box_faces(centre, half), {});
}

// Flat ground with a block standing on it.
StageCollision ground_with_block(const Vec3 centre, const float half) {
  std::vector<CollisionTriangle> faces = box_faces(centre, half);
  CollisionTriangle first, second;
  first.points = {Vec3{-100, 0, -100}, Vec3{-100, 0, 100}, Vec3{100, 0, 100}};
  second.points = {Vec3{-100, 0, -100}, Vec3{100, 0, 100}, Vec3{100, 0, -100}};
  first.normal = second.normal = {0, 1, 0};
  faces.push_back(first);
  faces.push_back(second);
  return StageCollision(std::move(faces), {});
}

void add_wreck(Battle &battle, const Vec3 centre) {
  RobotWreck wreck;
  wreck.centre = centre;
  wreck.radius = 2.0F;
  const auto index = battle.wrecks().items().size();
  battle.wrecks().add(wreck);
  battle.wrecks().set_collision(index, box(centre, 1.5F));
}

BattleSettings settings(const bool clearing) {
  BattleSettings result = parse_battle_settings(json::parse(R"({
    "arena": {"half_size": 100, "blocks": []},
    "weapons": {
      "gun": {"kind": "gun", "hud_kind": 2, "aim_mode": 0, "damage": 10, "cooldown": 0.05,
              "speed": 300, "range": 100}
    }
  })"));
  result.ai.fight = true;
  result.ai.original = parse_original_ai(
      DataRoots::shipped(PHANTOMCRASH_SOURCE_ROOT).document("world/robot_ai.json"));
  result.robot_wrecks.ai_clearing.enabled = clearing;
  return result;
}

// A computer robot facing -z, the player far behind it (out of its view).
struct Scene {
  Battle battle;
  int drone{};
  explicit Scene(const bool clearing) : battle(settings(clearing), {}) {
    battle.add_robot("player", 0, {0.0F, 0.0F, 80.0F}, 0.0F, {}, 1000.0F);
    battle.robot(0).frozen = true;
    drone = battle.add_robot("drone", 1, {}, 0.0F, {"gun", "", "", ""}, 100.0F);
    battle.robot(drone).drone = true; // fires (fire_drones)
  }
};

} // namespace

TEST_CASE("with nothing to fight, a computer robot turns to a wreck and shoots it apart") {
  Scene scene(true);
  add_wreck(scene.battle, {12.0F, 1.5F, -25.0F}); // ahead and to the right
  bool selected = false;
  float paid = 0;
  for (int frame = 0; frame < 60 * 10 && !scene.battle.wrecks().items()[0].removed; ++frame) {
    scene.battle.tick({}, {}, tick);
    selected = selected || scene.battle.robot(scene.drone).ai.wreck == 0;
    for (const auto &event : scene.battle.events()) {
      if (event.kind == BattleEventKind::wreck_destroyed) {
        CHECK(event.robot == scene.drone);
        paid += event.amount;
      }
    }
  }
  CHECK(selected);
  CHECK(scene.battle.wrecks().items()[0].removed);
  CHECK(paid == 0); // computer robots earn no wreck reward
}

TEST_CASE("without ai_clearing a computer robot leaves wrecks standing, as the original does") {
  Scene scene(false);
  add_wreck(scene.battle, {12.0F, 1.5F, -25.0F});
  for (int frame = 0; frame < 60 * 10; ++frame) {
    scene.battle.tick({}, {}, tick);
    CHECK(scene.battle.robot(scene.drone).ai.wreck == -1);
  }
  CHECK(scene.battle.wrecks().items()[0].solid());
}

TEST_CASE("a wreck out of range or behind a wall is not taken") {
  Scene scene(true);
  scene.battle.set_stage(ground_with_block({0.0F, 5.0F, -20.0F}, 6.0F)); // taller than the eye
  add_wreck(scene.battle, {0.0F, 1.5F, -30.0F});                         // behind the block
  add_wreck(scene.battle, {0.0F, 1.5F, 90.0F});                          // beyond the range
  for (int frame = 0; frame < 60 * 3; ++frame) {
    scene.battle.tick({}, {}, tick);
    CHECK(scene.battle.robot(scene.drone).ai.wreck == -1);
  }
}

TEST_CASE("an enemy in view comes before any wreck") {
  BattleSettings armed = settings(true);
  Battle battle(armed, {});
  const int player = battle.add_robot("player", 0, {0.0F, 0.0F, -20.0F}, 0.0F, {}, 1000.0F);
  battle.robot(player).frozen = true;
  const int drone = battle.add_robot("drone", 1, {}, 0.0F, {"gun", "", "", ""}, 100.0F);
  battle.robot(drone).drone = true;
  add_wreck(battle, {12.0F, 1.5F, -25.0F});
  for (int frame = 0; frame < 60 * 3; ++frame) {
    battle.tick({}, {}, tick);
    if (battle.robot(drone).ai.target >= 0) {
      CHECK(battle.robot(drone).ai.wreck == -1);
    }
  }
  CHECK(battle.robot(drone).ai.target == player);
  CHECK(battle.wrecks().items()[0].solid());
}

TEST_CASE("the shipped data lets computer robots clear wrecks") {
  const auto data = DataRoots::shipped(PHANTOMCRASH_SOURCE_ROOT).document("world/training.json");
  const auto battle = parse_battle_settings(*data.find("battle"));
  CHECK(battle.robot_wrecks.ai_clearing.enabled);
  CHECK(battle.robot_wrecks.ai_clearing.range > battle.robot_wrecks.ai_clearing.stand_off);
}
