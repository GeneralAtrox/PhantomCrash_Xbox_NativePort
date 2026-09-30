#include "test_support.hpp"
#include "phantomcrash/battle.hpp"
#include "phantomcrash/fence_impacts.hpp"

#include <cmath>
#include <numeric>

using namespace phantomcrash;

namespace {
FenceLook look() {
  FenceLook out;
  out.model = "models/binlink/197.gltf";
  out.meshes.resize(30);
  std::iota(out.meshes.begin(), out.meshes.end(), 0);
  return out;
}

StageCollision wall(const float x, const int surface) {
  CollisionTriangle t{{Vec3{x, 0, -50}, Vec3{x, 0, 50}, Vec3{x, 50, 0}}, {-1, 0, 0}, surface};
  return StageCollision({t}, {});
}

Landing shoot(const StageCollision &stage, const std::vector<Target> &targets = {}) {
  static const WeaponTuning gun = [] {
    WeaponTuning out;
    out.speed = 600;
    return out;
  }();
  Armory armory;
  armory.fire(gun, 0, 0, {0, 15, 0}, {1, 0, 0}, -1, targets);
  std::vector<WeaponHit> hits;
  armory.tick(1.0F / 30, targets, stage, hits);
  const auto landings = armory.take_landings();
  CHECK(landings.size() == 1);
  return landings[0];
}
} // namespace

TEST_CASE("shots above a fence retain its surface and normal at the impact") {
  const auto hit = shoot(wall(10, 1008));
  REQUIRE(hit.surface);
  CHECK(hit.surface->surface == 1008);
  CHECK(hit.surface->normal.x == -1);
  CHECK(std::abs(hit.at.x - 10) < 0.001F);
  CHECK(hit.at.y == 15);
  CHECK(!hit.on_robot);
}

TEST_CASE("an earlier wall or target does not inherit a fence hit behind it") {
  const auto fence = wall(10, 8);
  auto triangles = fence.walls();
  triangles.push_back(wall(5, 9).walls()[0]);
  const auto hit = shoot(StageCollision(triangles, {}));
  REQUIRE(hit.surface);
  CHECK(hit.surface->surface == 9);
  const auto robot = shoot(fence, {{{5, 15, 0}, 1, 1, true}});
  CHECK(robot.on_robot);
  CHECK(!robot.surface);
}

TEST_CASE("fence effects align to the hit plane and use the extracted scale") {
  FenceImpacts effects(look());
  effects.start({{10, 15, 0}, {-1, 0, 0}, 8});
  const auto frames = effects.frames();
  REQUIRE(frames.size() == 1);
  CHECK(frames[0].mesh == 0);
  const auto centre = transform_point({}, frames[0].world);
  CHECK(std::abs(centre.x - 10.5F) < 0.001F);
  CHECK(centre.y == 15);
  const auto normal = transform_point({0, 1, 0}, frames[0].world) - centre;
  CHECK(std::abs(normal.x + 0.2F) < 0.001F);
  CHECK(frames[0].refraction.radius == 0);
  CHECK(optical_seen(frames[0].refraction, centre) == 0);
}

TEST_CASE("fence animation advances at thirty frames per second independently of updates") {
  for (const int rate : {30, 60, 144}) {
    FenceImpacts effects(look());
    effects.start({{}, {0, 1, 0}, 8});
    for (int step = 0; step < rate / 2; ++step) {
      effects.tick(1.0F / rate);
    }
    const auto frames = effects.frames();
    REQUIRE(frames.size() == 1);
    CHECK(frames[0].mesh == 15);
    for (int step = 0; step < rate / 2 + 1; ++step) {
      effects.tick(1.0F / rate);
    }
    CHECK(effects.frames().empty());
  }
}

TEST_CASE("fence effects ignore ordinary surfaces and recycle their bounded pool") {
  FenceLook tuning = look();
  tuning.capacity = 2;
  FenceImpacts effects(tuning);
  effects.start({{}, {0, 1, 0}, 9});
  CHECK(effects.frames().empty());
  for (int i = 0; i < 3; ++i) {
    effects.start({{}, {0, 1, 0}, 8});
  }
  CHECK(effects.frames().size() == 2);
  effects.tick(1.1F);
  effects.start({{}, {0, 1, 0}, 8});
  CHECK(effects.frames().size() == 1);
}

TEST_CASE("fence effect data controls its frames timing and refraction") {
  const auto tuning = parse_fence_look(json::parse(R"({"model":"test.gltf","meshes":[3,5],
    "frame_rate":10,"capacity":4,"scale":2,"growth":1,"depth":0.5,"offset":0,
    "refraction":{"push":0.75,"reach":2,"edge":0.5,"least":0.1}})"));
  FenceImpacts effects(tuning);
  effects.start({{}, {0, 1, 0}, 8});
  effects.tick(0.1F);
  const auto frames = effects.frames();
  REQUIRE(frames.size() == 1);
  CHECK(frames[0].mesh == 5);
  CHECK(frames[0].refraction.push == 0.75F);
  effects.tick(0.1F);
  CHECK(effects.frames().empty());
}
