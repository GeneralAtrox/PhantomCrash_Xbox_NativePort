// The battle camera's views: the D-pad's order through them, the blend
// between first person and behind, where each view stands, and the fixed
// cameras' pick.

#include "test_support.hpp"

#include <cmath>
#include <vector>

#include "phantomcrash/battle_camera.hpp"
#include "phantomcrash/json.hpp"
#include "phantomcrash/robot_mover.hpp"

using namespace phantomcrash;

namespace {

constexpr float tick = 1.0F / 30.0F;

bool near(const Vec3 a, const Vec3 b) {
  return length(a - b) < 0.01F;
}

// A robot at the origin facing -z, its torso 3 up.
CameraSubject subject() {
  CameraSubject s;
  s.height = 1.5F;
  s.torso = {0.0F, 3.0F, 0.0F};
  s.legs_model = 3;
  return s;
}

const CameraShot chase{{0.0F, 5.0F, 10.0F}, {0.0F, 3.0F, -8.0F}, 55.0F, 0.0F};

} // namespace

// A robot standing at `feet`, its torso 3 up.
CameraSubject standing(const Vec3 feet, const float yaw = 0.0F) {
  CameraSubject s = subject();
  s.position = feet;
  s.torso = feet + Vec3{0.0F, 3.0F, 0.0F};
  s.yaw = yaw;
  return s;
}

TEST_CASE("the behind view stands over the torso and eases after the robot") {
  ChaseCamera camera(ChaseCameraSettings{});
  const StageCollision stage;
  const Vec3 start{320.0F, 20.0F, -480.0F};
  camera.tick(standing(start), stage, tick, true);
  // 0x07edfa: the eye 6.15 behind the torso and 2 up; the look point 8
  // ahead of the feet and 2 up.
  CHECK(near(camera.eye(), start + Vec3{0.0F, 5.0F, 6.15F}));
  CHECK(near(camera.target(), start + Vec3{0.0F, 2.0F, -8.0F}));

  // The eye closes a third of its gap a tick.
  const Vec3 travel{30.0F, 4.0F, -20.0F};
  const Vec3 before = camera.eye();
  camera.tick(standing(start + travel), stage, tick);
  CHECK(near(camera.eye(), before + travel * (1.0F / 3.0F)));
  const float turn = radians(90.0F);
  for (int frame = 0; frame < 90; ++frame) {
    camera.tick(standing(start + travel, turn), stage, tick);
  }
  const Vec3 forward = heading_forward(turn);
  const Vec3 feet = start + travel;
  CHECK(near(camera.eye(), feet + Vec3{0.0F, 5.0F, 0.0F} - forward * 6.15F));
  CHECK(near(camera.target(), feet + Vec3{0.0F, 2.0F, 0.0F} + forward * 8.0F));
}

TEST_CASE("an eye height sets the behind view's eye over the feet") {
  // The S.L.A.I. behind view measured under PCSX2: the eye 5 above and 8.6
  // behind the feet, the look point 0.5 above and 20 ahead of them.
  ChaseCameraSettings settings;
  settings.eye_height = 5.0F;
  settings.behind = 8.6F;
  settings.height = 0.5F;
  settings.look_ahead = 20.0F;
  ChaseCamera camera(settings);
  const StageCollision stage;
  const Vec3 start{-205.9F, 1.25F, 94.5F};
  camera.tick(standing(start), stage, tick, true);
  CHECK(near(camera.eye(), start + Vec3{0.0F, 5.0F, 8.6F}));
  CHECK(near(camera.target(), start + Vec3{0.0F, 0.5F, -20.0F}));

  // S.L.A.I. legs bring their own eye (0x335170 row 19: 9.5 behind, 5.4 up).
  CameraSubject legs = standing(start);
  legs.source_eye = SourceChaseEye{9.5F, 5.4F};
  camera.tick(legs, stage, tick, true);
  CHECK(near(camera.eye(), start + Vec3{0.0F, 5.4F, 9.5F}));
  CHECK(near(camera.target(), start + Vec3{0.0F, 0.5F, -20.0F}));

  // The Phantom Crash behind view ignores it.
  ChaseCamera crash{ChaseCameraSettings{}};
  crash.tick(legs, stage, tick, true);
  ChaseCamera plain{ChaseCameraSettings{}};
  plain.tick(standing(start), stage, tick, true);
  CHECK(near(crash.eye(), plain.eye()));

  const json::JsonValue row =
      json::parse(std::string_view{R"({"behind": 9.5, "eye_height": 5.4})"});
  const std::optional<SourceChaseEye> parsed = parse_source_chase_eye(&row);
  REQUIRE(parsed.has_value());
  CHECK(std::abs(parsed->behind - 9.5F) < 1e-5F);
  CHECK(std::abs(parsed->eye_height - 5.4F) < 1e-5F);
  CHECK(!parse_source_chase_eye(nullptr).has_value());
}

TEST_CASE("the S.L.A.I. air view lifts the eye and draws the look point in") {
  ChaseCameraSettings settings;
  settings.eye_height = 5.0F;
  settings.behind = 8.6F;
  settings.height = 0.5F;
  settings.look_ahead = 20.0F;
  settings.air = SourceAirView{2.0F, 0.75F, 0.05F, 4.0F};
  ChaseCamera camera(settings);
  const StageCollision stage;
  const Vec3 start{10.0F, 1.0F, 20.0F};
  camera.tick(standing(start), stage, tick, true);
  CHECK(near(camera.target(), start + Vec3{0.0F, 0.5F, -20.0F}));

  // Rising (0x260368): the look offset shrinks 0.05 a tick to 0.75 and the
  // eye stands 2 higher.
  CameraSubject rising = standing(start);
  rising.grounded = false;
  rising.climb = 3.0F;
  camera.tick(rising, stage, tick);
  CHECK(near(camera.target(), start + Vec3{0.0F, 0.5F, -20.0F} * 0.95F));
  for (int frame = 0; frame < 60; ++frame) {
    camera.tick(rising, stage, tick);
  }
  CHECK(near(camera.target(), start + Vec3{0.0F, 0.375F, -15.0F}));
  CHECK(near(camera.eye(), start + Vec3{0.0F, 7.0F, 8.6F}));

  // Back on the ground it eases back.
  camera.tick(standing(start), stage, tick);
  CHECK(near(camera.target(), start + Vec3{0.0F, 0.5F, -20.0F} * 0.8F));

  // Falling, it waits four ticks off the ground (0x26038c).
  ChaseCamera falling_camera(settings);
  falling_camera.tick(standing(start), stage, tick, true);
  CameraSubject falling = standing(start);
  falling.grounded = false;
  falling.climb = -3.0F;
  for (int frame = 0; frame < 3; ++frame) {
    falling_camera.tick(falling, stage, tick);
  }
  CHECK(near(falling_camera.target(), start + Vec3{0.0F, 0.5F, -20.0F}));
  falling_camera.tick(falling, stage, tick);
  CHECK(near(falling_camera.target(), start + Vec3{0.0F, 0.5F, -20.0F} * 0.95F));

  const auto parsed = parse_chase_camera(json::parse(
      std::string_view{R"({"air": {"rise": 2, "look_scale": 0.75, "ease": 0.05, "updates": 4}})"}));
  REQUIRE(parsed.air.has_value());
  CHECK(parsed.air->look_scale == 0.75F);
  CHECK(parsed.air->updates == 4.0F);
  CHECK(!parse_chase_camera(json::parse(std::string_view{"{}"})).air.has_value());
}

TEST_CASE("the S.L.A.I. eye stops off the stage and rises near a wall") {
  ChaseCameraSettings settings;
  settings.eye_height = 5.0F;
  settings.behind = 8.6F;
  settings.wall = SourceWallView{0.15F, 10.0F, 0.15F, 1.0F / 3.0F, 0.25F, 0.5F, 2.5F};
  settings.air = SourceAirView{2.0F, 0.75F, 0.05F, 4.0F};
  ArenaBounds bounds;
  bounds.half_size = 500.0F;
  const StageCollision stage = StageCollision::box(bounds, 30.0F);
  // The sight line from 4.4 up meets the wall at z 500, 3 of its 8.6 out.
  const float wall_y = 4.4F + 0.6F * 3.0F / 8.6F;
  const Vec3 feet{200.0F, 0.0F, 497.0F};
  const auto settle = [&](const CameraSubject &robot) {
    ChaseCamera camera(settings);
    camera.tick(robot, stage, tick, true);
    for (int frame = 0; frame < 120; ++frame) {
      camera.tick(robot, stage, tick);
    }
    return camera.eye();
  };
  ChaseCamera snapped(settings);
  snapped.tick(standing(feet), stage, tick, true);
  CHECK(near(snapped.eye(), {200.0F, wall_y, 499.85F}));
  // Each tick the vertical gap closes a ninth and the eye rises a third of
  // 0.25: it settles 0.75 above the wall point.
  CHECK(near(settle(standing(feet)), {200.0F, wall_y + 0.75F, 499.85F}));
  // Legs row 31 rises 0.35.
  CameraSubject row31 = standing(feet);
  row31.source_eye = SourceChaseEye{8.6F, 5.0F, std::nullopt, 0.35F};
  CHECK(near(settle(row31), {200.0F, wall_y + 1.05F, 499.85F}));
  // In the air 0.5 more, plus 2.5 times the climb a tick; the sight line
  // starts a leg radius lower and the eye stands 2 higher.
  CameraSubject airborne = standing(feet);
  airborne.grounded = false;
  airborne.climb = 3.0F;
  const float air_y = settle(airborne).y;
  CHECK(std::abs(air_y - (4.4F - 1.5F + 4.1F * 3.0F / 8.6F + 3.0F * 1.0F)) < 0.01F);

  // Away from walls the eye just closes a third of its gap; row 33 keeps it
  // 1.2 above the feet.
  ChaseCamera open(settings);
  CameraSubject low = standing({0.0F, 0.0F, 0.0F});
  low.source_eye = SourceChaseEye{8.6F, 0.5F, 1.2F, std::nullopt};
  open.tick(low, stage, tick, true);
  CHECK(near(open.eye(), {0.0F, 1.2F, 8.6F}));
  open.tick(standing({0.0F, 0.0F, -3.0F}), stage, tick);
  CHECK(near(open.eye(), {0.0F, 1.2F + 3.8F / 3.0F, 8.6F - 1.0F}));

  const auto parsed = parse_chase_camera(json::parse(std::string_view{
      R"({"wall": {"margin": 0.15, "reach": 10, "normal_limit": 0.15, "ease": 0.333333,
          "rise": 0.25, "air_rise": 0.5, "air_climb": 2.5}})"}));
  REQUIRE(parsed.wall.has_value());
  CHECK(parsed.wall->reach == 10.0F);
  CHECK(parsed.wall->air_climb == 2.5F);
  const auto row =
      json::parse(std::string_view{R"({"behind": 9, "eye_height": 1, "wall_rise": 0.35})"});
  const auto eye = parse_source_chase_eye(&row);
  REQUIRE(eye.has_value());
  CHECK(eye->wall_rise == 0.35F);
  CHECK(!eye->min_eye_height.has_value());
}

TEST_CASE("the behind view rises in the air and keeps off walls") {
  ChaseCamera camera(ChaseCameraSettings{});
  ArenaBounds bounds;
  bounds.half_size = 500.0F;
  const StageCollision stage = StageCollision::box(bounds, 30.0F);
  camera.tick(standing({200.0F, 0.0F, 497.0F}), stage, tick, true);
  CHECK(std::abs(camera.eye().x - 200.0F) < 0.01F);
  CHECK(camera.eye().z < 500.0F);
  CHECK(camera.eye().z > 497.0F);
  CHECK(!stage.segment_hit(camera.anchor(), camera.eye()));
  for (int frame = 0; frame < 90; ++frame) {
    camera.tick(standing({200.0F, 0.0F, 450.0F}), stage, tick);
  }
  CHECK(near(camera.eye(), {200.0F, 5.0F, 456.15F}));

  // Off the ground the eye rises the robot's top less half a leg radius.
  CameraSubject jumping = standing({200.0F, 0.0F, 450.0F});
  jumping.grounded = false;
  camera.tick(jumping, stage, tick, true);
  const ChaseCameraSettings defaults;
  CHECK(std::abs(camera.eye().y - (5.0F + defaults.robot_top - 0.75F)) < 0.01F);
}

TEST_CASE("the D-pad walks the orbit views in the original's order") {
  CameraViews views(CameraViewSettings{}, {});
  CHECK(views.view() == camera_view::behind);
  CHECK(!views.change(ViewChange::behind));
  std::vector<int> order;
  for (int index = 0; index < 5; ++index) {
    CHECK(views.change(ViewChange::next));
    order.push_back(views.view());
  }
  CHECK((order == std::vector<int>{2, 3, 4, 5, 2}));
  order.clear();
  for (int index = 0; index < 4; ++index) {
    CHECK(views.change(ViewChange::previous));
    order.push_back(views.view());
  }
  CHECK((order == std::vector<int>{5, 4, 3, 2}));
  CameraViews from_behind(CameraViewSettings{}, {});
  CHECK(from_behind.change(ViewChange::previous));
  CHECK(from_behind.view() == camera_view::fixed);
}

TEST_CASE("first person and behind blend over twenty ticks and hold the buttons meanwhile") {
  CameraViews views(CameraViewSettings{}, {});
  const CameraSubject robot = subject();
  views.tick(robot, chase, tick, {}, true);
  CHECK(near(views.shot().eye, chase.eye));
  CHECK(views.change(ViewChange::first_person));
  CHECK(views.first_person());
  // Legs model 3: 1.0 above and 1.6 ahead of the torso, looking 24 ahead.
  const Vec3 cockpit{0.0F, 4.0F, -1.6F};
  views.tick(robot, chase, tick, {});
  CHECK(near(views.shot().eye, chase.eye)); // the whole of the old view's share
  CHECK(!views.change(ViewChange::behind));
  for (int index = 0; index < 10; ++index) {
    views.tick(robot, chase, tick, {});
  }
  CHECK(near(views.shot().eye, (chase.eye + cockpit) * 0.5F));
  for (int index = 0; index < 10; ++index) {
    views.tick(robot, chase, tick, {});
  }
  CHECK(near(views.shot().eye, cockpit));
  CHECK(near(views.shot().target, {0.0F, 1.5F, -24.0F}));
  CHECK(views.shot().fov_degrees == 52.0F);
  CHECK(views.change(ViewChange::behind));
}

TEST_CASE("the orbit views stand beside and ahead of the robot and look at its torso") {
  CameraViews views(CameraViewSettings{}, {});
  CameraSubject robot = subject();
  robot.yaw = pi * 0.5F; // facing -x
  robot.pitch = 0.25F;
  views.tick(robot, chase, tick, {}, true);
  CHECK(views.change(ViewChange::next));
  views.tick(robot, chase, tick, {});
  // Local -x is the robot's right.
  CHECK(near(views.shot().eye, Vec3{0.0F, 1.65F, 0.0F} + heading_right(robot.yaw) * 5.5F));
  CHECK(near(views.shot().target, {0.0F, 4.5F, 0.0F}));
  CHECK(views.change(ViewChange::next));
  views.tick(robot, chase, tick, {});
  CHECK(near(views.shot().eye, Vec3{0.0F, 5.5F, 0.0F} - heading_right(robot.yaw) * 8.0F));
  CHECK(views.change(ViewChange::next));
  views.tick(robot, chase, tick, {});
  CHECK(near(views.shot().eye, Vec3{0.0F, 2.15F, 0.0F} + heading_forward(robot.yaw) * 8.0F));
}

TEST_CASE("a fixed camera is the nearest point that sees the robot") {
  const auto document = json::parse(R"({"fixed_cameras": {"6": {"points":
    [[100, 5, 0], [10, 5, 0], [20, 5, 0], [0, 5, 30]], "areas": [[0, 4]]}}})");
  const std::vector<Vec3> points = parse_fixed_cameras(document, 6);
  CHECK(points.size() == 4);
  CHECK(parse_fixed_cameras(document, 2).empty());
  CameraViews views(CameraViewSettings{}, points);
  const CameraSubject robot = subject();
  CHECK(views.change(ViewChange::previous));
  bool wall = true; // a wall hides the nearest point
  const SightTest sees = [&](const Vec3 from, Vec3) { return !(wall && from.x == 10.0F); };
  views.tick(robot, chase, tick, sees);
  CHECK(near(views.shot().eye, {20.0F, 5.0F, 0.0F}));
  wall = false;
  for (int index = 0; index < 29; ++index) {
    views.tick(robot, chase, tick, sees);
  }
  CHECK(near(views.shot().eye, {20.0F, 5.0F, 0.0F})); // it looks again after 30 ticks
  views.tick(robot, chase, tick, sees);
  CHECK(near(views.shot().eye, {10.0F, 5.0F, 0.0F}));
  const auto settings = parse_camera_views(json::parse(R"({"fov_degrees": [40, 52, 52, 52, 52, 60],
    "start_view": 0, "orbits": [{"offset": [1, 2, 3]}, {"offset": [0, 0, 0]},
    {"offset": [0, 0, 0], "pitch_lift": true}]})"));
  CHECK(settings.fov_degrees[0] == 40.0F && settings.fov_degrees[5] == 60.0F);
  CHECK(settings.start_view == camera_view::first_person);
  CHECK(settings.orbits[0].offset.z == 3.0F && settings.orbits[2].pitch_lift);
}
