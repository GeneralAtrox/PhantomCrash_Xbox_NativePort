#include "test_support.hpp"
#include "phantomcrash/battle.hpp"
#include "phantomcrash/supply_drop.hpp"

#include <algorithm>
#include <cmath>

using namespace phantomcrash;

namespace {
constexpr float tick = 1.0F / 30.0F;
float ground(Vec3) {
  return 0;
}

SupplySettings delivery() {
  SupplySettings settings;
  SupplyRoute route;
  for (int i = 0; i < 4; ++i) {
    const float x = static_cast<float>(i * 30);
    route.segments.push_back(
        {Vec3{x, 30, 0}, Vec3{x + 10, 30, 0}, Vec3{x + 20, 30, 0}, Vec3{x + 30, 30, 0}});
  }
  route.closing = 1;
  route.drop = 2;
  route.leave = 3;
  settings.routes.push_back(route);
  return settings;
}
} // namespace

TEST_CASE("destroying a carrier removes cargo immediately and the wreck sinks with reduced drift") {
  const auto settings = delivery();
  SupplyDrop drop(settings, 7);
  drop.tick(tick, ground);
  const Vec3 before = drop.carrier().position;
  drop.tick(tick, ground);
  const Vec3 shot = drop.carrier().position;
  REQUIRE(drop.shoot_carrier());
  CHECK(std::ranges::all_of(drop.pickups(),
                            [](const auto &p) { return p.state == SupplyState::gone; }));
  drop.tick(tick, ground);
  CHECK(drop.carrier().wrecked);
  CHECK(std::abs(drop.carrier().position.y - (shot.y - settings.wreck_sink)) < 0.001F);
  CHECK(std::abs(drop.carrier().position.x - shot.x - (shot.x - before.x) * settings.wreck_keep) <
        0.001F);
  CHECK(!drop.shoot_carrier());
}

TEST_CASE("released falling and landed pickups survive the carrier and its final explosion") {
  SupplyDrop drop(delivery(), 7);
  for (int i = 0; i < 160; ++i) {
    drop.tick(tick, ground);
  }
  REQUIRE(drop.pickups()[0].state == SupplyState::landed);
  REQUIRE(drop.pickups()[1].state == SupplyState::falling);
  REQUIRE(drop.pickups()[2].state == SupplyState::carried);
  const Vec3 falling = drop.pickups()[1].position;
  REQUIRE(drop.shoot_carrier());
  CHECK(drop.pickups()[0].state == SupplyState::landed);
  CHECK(drop.pickups()[1].state == SupplyState::falling);
  CHECK(drop.pickups()[2].state == SupplyState::gone);
  drop.tick(tick, ground);
  CHECK(drop.pickups()[1].position.x == falling.x);
  CHECK(drop.pickups()[1].position.y < falling.y);
  std::optional<Vec3> crash;
  for (int i = 0; i < 120 && !crash; ++i) {
    drop.tick(tick, ground);
    crash = drop.take_crash();
  }
  REQUIRE(crash);
  CHECK(drop.carrier().parked);
  CHECK(drop.pickups()[0].state == SupplyState::landed);
  CHECK(drop.pickups()[1].state == SupplyState::landed);
  CHECK(drop.pickups()[2].state == SupplyState::gone);
  CHECK(drop.take(drop.pickups()[1].position, 1).has_value());
}

TEST_CASE("a wreck explodes at a stage obstruction while still above the ground") {
  SupplyDrop drop(delivery(), 7);
  drop.tick(tick, ground);
  REQUIRE(drop.shoot_carrier());
  const Vec3 contact{1, 29.7F, 0};
  drop.tick(tick, ground, [&](Vec3, Vec3, float radius) -> std::optional<Vec3> {
    CHECK(radius == 3);
    return contact;
  });
  const auto crash = drop.take_crash();
  REQUIRE(crash);
  CHECK(length(*crash - contact) < 0.001F);
  CHECK(!drop.take_crash());
  CHECK(!drop.carrier().wrecked);
}

TEST_CASE("a wreck times out in midair when there is no surface to hit") {
  auto settings = delivery();
  settings.wreck_ticks = 1;
  SupplyDrop drop(settings, 7);
  REQUIRE(drop.shoot_carrier());
  drop.tick(tick, ground);
  CHECK(!drop.take_crash());
  drop.tick(tick, ground);
  const auto crash = drop.take_crash();
  REQUIRE(crash);
  CHECK(crash->y > 20);
}

TEST_CASE("shooting the carrier emits its explosion in the hit frame and again on landing") {
  BattleSettings settings;
  WeaponTuning gun;
  gun.kind = WeaponKind::gun;
  gun.speed = 600;
  settings.weapons.emplace("gun", gun);
  Battle battle(settings, {});
  battle.add_robot("player", 0, {}, 0, {"gun", "", "", ""}, 100);
  auto supply = delivery();
  supply.speed = 0;
  const Vec3 at{0, 4, -15};
  for (auto &segment : supply.routes[0].segments) {
    segment.fill(at);
  }
  battle.set_supply(supply);
  FireControl fire;
  fire.fire[0] = true;
  battle.tick({}, fire, tick);
  REQUIRE(battle.supply()->carrier().wrecked);
  const auto explosion_count = [&] {
    return std::ranges::count_if(battle.events(), [&](const auto &event) {
      return event.kind == BattleEventKind::effect && event.slot == supply.look.crash_effect;
    });
  };
  CHECK(explosion_count() == 1);
  CHECK(std::ranges::any_of(battle.events(), [&](const auto &event) {
    return event.kind == BattleEventKind::effect && length(event.at - at) < 0.001F;
  }));
  CHECK(std::ranges::all_of(battle.supply()->pickups(),
                            [](const auto &p) { return p.state == SupplyState::gone; }));
  int later_explosions = 0;
  for (int i = 0; i < 10; ++i) {
    battle.tick({}, {}, tick);
    later_explosions += static_cast<int>(explosion_count());
  }
  CHECK(later_explosions == 1);
}
