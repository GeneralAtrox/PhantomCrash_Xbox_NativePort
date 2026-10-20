#include "test_support.hpp"
#include "phantomcrash/skeleton.hpp"
#include "phantomcrash/supply_drop.hpp"
#include "phantomcrash/world_model.hpp"

#include <cmath>

using namespace phantomcrash;

namespace {

// The importer's values for the PAL carrier (supply_assets.py), world scale 1.
constexpr const char *source_look = R"({
  "model": "crate.gltf",
  "images": ["0", "1", "2", "3", "4", "5", "6", "7", "8", "9"],
  "carrier_model": "jet01.gltf",
  "pulse": {"frame_rate": 30, "step": 0.05, "fast_frames": 60, "fast_rate": 15,
            "life_numerator": 900, "colour_base": 0.5, "colour_steps": 96, "colour_divisor": 64},
  "flight": {
    "update_rate": 30, "speed": 0.015, "release_delay": 100, "lifetime": 900,
    "burst_hold": 15, "fall": 0.5, "rest": 1, "hatch_open": 0.7853982, "hatch_speed": 0.03,
    "light_step": 0.03, "flight_light_step": 0.03,
    "joint_controls": [12, 16, 9, 13, 10, 14, 0],
    "wing_swing": [[0.9948, 0.6458, 0.1920], [0.9948, -0.6458, -0.1920]],
    "wreck_pose": [[-0.12217, -0.03491, -0.10472], [-0.31416, -0.61087, 0.13963],
                   [-0.97738, -0.43633, -0.43633], [-0.06981, -0.48869, -0.05236],
                   [-1.0472, 0.03491, 0.27925], [-0.48869, -0.40143, 0], [-0.34907, -0.08727, -0.20944]],
    "wreck_ease": 0.01, "wreck_keep": 0.6, "wreck_sink": 0.3, "wreck_accel": 1.01,
    "wreck_updates": 500, "wreck_shade": 0.4, "carrier_radius": 3, "pickup_radius": 1,
    "respawn": 30}
})";

constexpr float tick = 1.0F / 30.0F;

bool near(const double a, const double b) {
  return std::fabs(a - b) < 1e-4;
}

SupplySettings source_settings(const std::optional<float> floor = std::nullopt) {
  SupplySettings settings;
  settings.look.pickups = parse_supply_pickup_look(json::parse(source_look));
  SupplyRoute route;
  for (int index = 0; index < 6; ++index) {
    const float z = 10.0F * static_cast<float>(index);
    route.segments.push_back(
        {Vec3{0, 10, z}, Vec3{0, 10, z + 3}, Vec3{0, 10, z + 7}, Vec3{0, 10, z + 10}});
  }
  route.closing = 1;
  route.drop = 3;
  route.leave = 4;
  route.floor = floor;
  settings.routes = {route};
  settings.stage_routes = true;
  return settings;
}

void run(SupplyDrop &drop, const int ticks, const float ground = 0.0F) {
  for (int index = 0; index < ticks; ++index) {
    drop.tick(tick, [ground](Vec3) { return ground; });
  }
}

} // namespace

TEST_CASE("the source flight replaces the carrier's speeds, counts and radii") {
  SupplyDrop drop(source_settings(), 1);
  const auto &s = drop.settings();
  REQUIRE(drop.flight() != nullptr);
  CHECK(s.tick_rate == 30);
  CHECK(near(s.speed, 0.015));
  CHECK(s.drop_ticks == 100);
  CHECK(s.lifetime_ticks == 900);
  CHECK(s.wreck_ticks == 500);
  CHECK(s.hold.x == 0);
  CHECK(s.hold.y == 0);
  CHECK(s.hold.z == 0);
  CHECK_THROWS(parse_supply_source_flight(json::parse(R"({"update_rate": 30})")));
}

TEST_CASE("the wings swing open over 1 / speed updates from the closing segment") {
  SupplyDrop drop(source_settings(), 1);
  run(drop, 67); // the 67th update crosses into segment 1, the closing segment
  CHECK(drop.carrier().joints[0].x == 0);
  run(drop, 1);
  CHECK(near(drop.carrier().joints[0].x, 0.9948 * 0.015));
  run(drop, 100);
  const auto &joints = drop.carrier().joints;
  CHECK(near(joints[0].x, 0.9948 * 67 * 0.015));
  CHECK(near(joints[1].y, -0.6458 * 67 * 0.015));
  for (std::size_t joint = 2; joint < joints.size(); ++joint) {
    CHECK(joints[joint].x == 0);
  }
}

TEST_CASE("crates land on the route's floor and stay drawn through their burst") {
  SupplyDrop drop(source_settings(-5.0F), 1);
  run(drop, 201 + 27 + 40);
  const SupplyPickup &first = drop.pickups()[0];
  REQUIRE(first.state == SupplyState::landed);
  CHECK(near(first.position.y, -4.0)); // floor + rest, not the ground's 1
  CHECK(drop.pickups()[1].state == SupplyState::carried);
  drop.shoot_pickup(0);
  run(drop, 1);
  CHECK(drop.take_bursts().size() == 1);
  CHECK(SupplyDrop::visible(drop.pickups()[0]));
  run(drop, 14);
  CHECK(SupplyDrop::visible(drop.pickups()[0]));
  run(drop, 1);
  CHECK(drop.pickups()[0].state == SupplyState::gone);
  CHECK(drop.take_bursts().empty());
}

TEST_CASE("the light turns twice as fast in flight as elsewhere") {
  SupplyDrop drop(source_settings(), 1);
  run(drop, 1);
  CHECK(near(drop.carrier().spin, 0.06));
  run(drop, 67);
  const float before = drop.carrier().spin;
  run(drop, 1); // swinging the wings, state 2
  CHECK(near(drop.carrier().spin - before, 0.03));
}

TEST_CASE("a shot-down source carrier shades, speeds up and eases into its fall pose") {
  SupplyDrop drop(source_settings(), 1);
  run(drop, 10);
  REQUIRE(drop.shoot_carrier());
  CHECK(near(drop.carrier().shade, 0.4));
  const Vec3 a = drop.carrier().position;
  run(drop, 1, -1000.0F);
  const Vec3 b = drop.carrier().position;
  run(drop, 1, -1000.0F);
  const Vec3 c = drop.carrier().position;
  CHECK(near(a.y - b.y, 0.3));
  CHECK(near(b.y - c.y, 0.3 * 1.01));
  CHECK(near(c.z - b.z, (b.z - a.z) * 1.01));
  CHECK(near(drop.carrier().joints[2].x, -0.02)); // .01 an update toward -0.977
  CHECK(drop.carrier().joints[5].z == 0);         // already at its pose
}

TEST_CASE("an emptied source carrier waits its respawn count before setting out") {
  SupplyDrop drop(source_settings(), 1);
  run(drop, 1);
  REQUIRE(drop.shoot_carrier());
  run(drop, 1, 9.5F); // the wreck strikes the ground at once and parks
  REQUIRE(drop.carrier().parked);
  run(drop, 29);
  CHECK(drop.carrier().parked);
  run(drop, 1);
  CHECK(!drop.carrier().parked);
  CHECK(drop.carrier().shade == 1);
}

TEST_CASE("a joint turns its control about its own pivot and control 0 turns the carrier") {
  ModelData model;
  ModelNode wing;
  wing.name = "source_node_12";
  wing.parent_local = translation({2, 0, 0});
  ModelNode tip;
  tip.name = "source_node_11";
  tip.parent = 0;
  tip.parent_local = translation({1, 0, 0});
  model.nodes = {wing, tip};
  SupplySourceFlight flight;
  flight.joint_controls = {12, 16, 9, 13, 10, 14, 0};
  SupplyJoints joints{};
  joints[0] = {0, pi / 2, 0}; // the wing's yaw
  const auto worlds = supply_carrier_nodes(model, flight, joints, translation({0, 5, 0}));
  REQUIRE(worlds.size() == 2);
  const Vec3 pivot = transform_point({}, worlds[0]);
  CHECK(near(pivot.x, 2));
  CHECK(near(pivot.y, 5));
  const Vec3 end = transform_point({}, worlds[1]);
  const Vec3 turned = transform_point({1, 0, 0}, rotation_y(pi / 2));
  CHECK(near(end.x, 2 + turned.x));
  CHECK(near(end.z, turned.z));
  joints = {};
  joints[6] = {0, pi, 0}; // the whole carrier
  const auto body = supply_carrier_nodes(model, flight, joints, Mat4{});
  CHECK(near(transform_point({}, body[0]).x, -2));
}
