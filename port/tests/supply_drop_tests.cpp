// The supply drops: the carrier's route, its drop, the pickups' fall,
// lifetime and pick-up, and the kinds' parse.

#include "test_support.hpp"

#include <cmath>

#include "phantomcrash/json.hpp"
#include "phantomcrash/fixed_step.hpp"
#include "phantomcrash/supply_drop.hpp"

using namespace phantomcrash;

namespace {

bool near(const float a, const float b) {
  return std::fabs(a - b) < 0.01F;
}

// A straight route of four segments along +x at height 30: closing in on
// segment 1, dropping on 2, turning away on 3.
SupplySettings test_settings() {
  SupplySettings settings;
  SupplyRoute route;
  for (int index = 0; index < 4; ++index) {
    const auto x = static_cast<float>(index * 30);
    route.segments.push_back({Vec3{x, 30.0F, 0.0F}, Vec3{x + 10.0F, 30.0F, 0.0F},
                              Vec3{x + 20.0F, 30.0F, 0.0F}, Vec3{x + 30.0F, 30.0F, 0.0F}});
  }
  route.closing = 1;
  route.drop = 2;
  route.leave = 3;
  settings.routes.push_back(route);
  settings.chances = {0.0F, 0.0F, 0.0F, 0.0F, 1.0F, 0.0F, 0.0F, 0.0F, 0.0F, 0.0F};
  return settings;
}

float flat(const Vec3) {
  return 0.0F;
}

constexpr float tick = 1.0F / 30.0F;

} // namespace

TEST_CASE("a Bezier segment runs from its first point to its last") {
  const std::array<Vec3, 4> segment{Vec3{0, 0, 0}, Vec3{0, 3, 0}, Vec3{3, 3, 0}, Vec3{3, 0, 0}};
  CHECK(near(bezier_point(segment, 0.0F).x, 0.0F));
  CHECK(near(bezier_point(segment, 1.0F).x, 3.0F));
  CHECK(near(bezier_point(segment, 0.5F).y, 2.25F));
}

TEST_CASE("the carrier drops its pickups one after another on the drop segment") {
  SupplyDrop drop(test_settings(), 7);
  CHECK(drop.active());
  if (!(drop.active())) {
    return;
  }
  CHECK(drop.pickups().size() == 3);
  CHECK(drop.pickups()[0].kind == 4); // the only kind with a chance
  // 0.03 of a segment a tick: two segments take 67 ticks, then the hatch
  // opens over 27 more before the first pickup falls.
  int ticks = 0;
  while (drop.pickups()[0].state == SupplyState::carried && ticks < 200) {
    drop.tick(tick, flat);
    ++ticks;
  }
  CHECK(ticks >= 92);
  CHECK(ticks <= 96);
  CHECK(drop.pickups()[1].state == SupplyState::carried);
  for (int index = 0; index < 60; ++index) {
    drop.tick(tick, flat);
  }
  CHECK(drop.pickups()[0].state == SupplyState::landed);
  CHECK(near(drop.pickups()[0].position.y, 1.0F)); // rests one above the ground
  CHECK(drop.pickups()[1].state != SupplyState::carried);
}

TEST_CASE("a landed pickup is taken by a robot touching it, or bursts in time") {
  SupplyDrop drop(test_settings(), 7);
  for (int index = 0; index < 400; ++index) {
    drop.tick(tick, flat);
  }
  const SupplyPickup first = drop.pickups()[0];
  CHECK(first.state == SupplyState::landed);
  if (!(first.state == SupplyState::landed)) {
    return;
  }
  CHECK(!drop.take(first.position + Vec3{4.0F, 0.0F, 0.0F}, 2.0F).has_value());
  const auto taken = drop.take(first.position + Vec3{2.5F, 0.0F, 0.0F}, 2.0F);
  CHECK(taken.has_value());
  if (!(taken.has_value())) {
    return;
  }
  CHECK(taken->kind.effect == SupplyEffect::repair && near(taken->kind.amount, 0.1F));
  CHECK(taken->at.x == first.position.x && taken->type == first.kind);
  CHECK(drop.pickups()[0].state == SupplyState::gone);
  // The others wait out 900 ticks, then burst; the parked carrier then sets
  // out again with a fresh load.
  for (int index = 0; index < 901; ++index) {
    drop.tick(tick, flat);
  }
  CHECK(!drop.carrier().parked);
  CHECK(drop.pickups()[2].state == SupplyState::carried);
}

TEST_CASE("a shot brings the carrier down, and it crashes and sets out again") {
  SupplyDrop drop(test_settings(), 7);
  for (int index = 0; index < 20; ++index) {
    drop.tick(tick, flat);
  }
  CHECK(drop.carrier_shootable());
  CHECK(drop.shoot_carrier());
  CHECK(!drop.shoot_carrier()); // once
  CHECK(drop.carrier().wrecked);
  // Sinking 0.3 a tick from 30 up, it strikes the ground inside 100 ticks.
  std::optional<Vec3> crash;
  for (int index = 0; index < 100 && !crash; ++index) {
    drop.tick(tick, flat);
    crash = drop.take_crash();
  }
  CHECK(crash.has_value());
  // Its hold was lost with it: it sets out again at once, loaded afresh.
  drop.tick(tick, flat);
  CHECK(!drop.carrier().parked && !drop.carrier().wrecked);
  CHECK(drop.pickups()[0].state == SupplyState::carried);
}

TEST_CASE("the supply settings and routes parse") {
  const SupplySettings settings = parse_supply_settings(json::parse(R"({
    "speed": 0.05, "lifetime_ticks": 600, "look": {"burst_effect": 65},
    "kinds": [{"effect": "money", "amount": 1}, {"effect": "money", "amount": 2},
              {"effect": "money", "amount": 3}, {"effect": "money", "amount": 4},
              {"effect": "repair", "amount": 0.2}, {"effect": "repair", "amount": 0.3},
              {"effect": "repair", "amount": 0.4}, {"effect": "ammo", "amount": 0.5},
              {"effect": "ammo", "amount": 0.6}, {"effect": "ammo", "amount": 0.7}]})"));
  CHECK(near(settings.speed, 0.05F) && near(settings.lifetime_ticks, 600.0F));
  CHECK(settings.look.burst_effect == 65);
  CHECK(SupplySettings{}.look.burst_effect == 0x40);
  CHECK(settings.kinds[9].effect == SupplyEffect::ammo && near(settings.kinds[9].amount, 0.7F));
  const auto routes = parse_supply_routes(
      json::parse(R"({"supply_routes": {"6": [{"segments": [[[0,0,0],[1,0,0],[2,0,0],[3,0,0]]],
                      "closing": 0, "drop": 0, "leave": 0}]}})"),
      6);
  CHECK(routes.size() == 1);
  if (!(routes.size() == 1)) {
    return;
  }
  CHECK(near(routes[0].segments[0][3].x, 3.0F));
  CHECK(parse_supply_routes(json::parse(R"({})"), 6).empty());
}

TEST_CASE("dropship travel follows the original 30 Hz movement step at any presentation rate") {
  // 0x066940 passes 0x3cf5c28f (0.03) to 0x0679e7. 0x067bf6,
  // 0x067f04 and 0x06804b add it once per 30 Hz battle update.
  // Use a long straight inbound route, so no hatch/parking transitions
  // hide a doubled update or a presentation-dependent movement rate.
  auto settings = test_settings();
  auto &route = settings.routes[0];
  route.segments.clear();
  route.closing = route.drop = route.leave = 100;
  for (int i = 0; i < 20; ++i) {
    const float x = static_cast<float>(i * 30);
    route.segments.push_back(
        {Vec3{x, 30, 0}, Vec3{x + 10, 30, 0}, Vec3{x + 20, 30, 0}, Vec3{x + 30, 30, 0}});
  }
  REQUIRE(near(settings.speed, 0.03F));
  REQUIRE(near(settings.tick_rate, 30));
  SupplyDrop source(settings, 7);
  for (int i = 0; i < 300; ++i) {
    source.tick(1.0F / 30.0F, flat);
  }
  CHECK(std::abs(source.carrier().position.x - 270.0F) < 0.01F);
  // The desktop runs fixed 60 Hz logic even at unlimited rendering.
  // 480 Hz exercises that case; irregular presentation is covered below.
  for (const int fps : {30, 60, 120, 144, 480}) {
    SupplyDrop rendered(settings, 7);
    FixedStep clock(1.0 / 60.0);
    int ticks = 0;
    for (int frame = 0; frame < fps * 10; ++frame) {
      const int due = clock.add(1.0 / fps);
      for (int i = 0; i < due; ++i) {
        rendered.tick(static_cast<float>(clock.tick_seconds()), flat);
      }
      ticks += due;
    }
    CHECK(ticks >= 599 && ticks <= 600); // floating-point remainder: at most one host tick
    CHECK(std::abs(rendered.carrier().position.x - source.carrier().position.x) < 0.46F);
  }
  SupplyDrop uncapped(settings, 7);
  FixedStep clock(1.0 / 60.0);
  double elapsed = 0;
  int frame = 0;
  while (elapsed < 10) {
    const double delta = std::min(10 - elapsed, frame++ % 2 == 0 ? 0.001 : 0.012);
    elapsed += delta;
    for (int i = clock.add(delta); i > 0; --i) {
      uncapped.tick(static_cast<float>(clock.tick_seconds()), flat);
    }
  }
  CHECK(std::abs(uncapped.carrier().position.x - source.carrier().position.x) < 0.46F);
}
