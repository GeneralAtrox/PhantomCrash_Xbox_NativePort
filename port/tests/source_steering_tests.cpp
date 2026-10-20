// S.L.A.I. legs' steering inputs: the Standard stick filter (0x4debd8), the
// turn motion's playback rate (0x4ebc28), the graded load factor
// (0x2568d0) and the turn speed it loads (0x4dd590).

#include "test_support.hpp"

#include "phantomcrash/robot_load.hpp"
#include "phantomcrash/robot_mover.hpp"
#include "phantomcrash/source_load.hpp"
#include "phantomcrash/source_roll.hpp"
#include "phantomcrash/source_steering.hpp"

#include <cmath>

using namespace phantomcrash;

namespace {

bool close_to(const float a, const float b) {
  return std::fabs(a - b) < 1e-5F;
}

RobotControl standard(const float turn, const float aim) {
  RobotControl control;
  control.standard = true;
  control.move[0] = turn;
  control.pitch = aim;
  return control;
}

LevelCurve flat(const float value) {
  LevelCurve curve;
  curve.base = value;
  return curve;
}

// A 60 body on 40 legs that turn 2 degrees a vsync.
PartTables slai_tables(const float capacity) {
  PartTables tables;
  ArmourRecord body;
  body.weight = flat(60.0F);
  tables.bodies.push_back(body);
  ArmourRecord legs;
  legs.weight = flat(40.0F);
  legs.capacity = flat(capacity);
  legs.turn = flat(2.0F);
  legs.source_load = SourceLoadRule{};
  tables.legs.push_back(legs);
  return tables;
}

} // namespace

TEST_CASE("the standard stick drops the turn near its aim axis and the aim near its turn axis") {
  auto near_aim = standard(0.3F, 0.9F); // 18 degrees off the aim axis
  apply_source_steering(near_aim);
  CHECK(near_aim.move[0] == 0.0F);
  CHECK(near_aim.pitch == 0.9F);
  auto near_turn = standard(0.9F, -0.2F); // 77 degrees
  apply_source_steering(near_turn);
  CHECK(near_turn.move[0] == 0.9F);
  CHECK(near_turn.pitch == 0.0F);
  auto diagonal = standard(-0.5F, 0.5F);
  apply_source_steering(diagonal);
  CHECK(diagonal.move[0] == -0.5F);
  CHECK(diagonal.pitch == 0.5F);
  auto pure = standard(1.0F, 0.0F);
  apply_source_steering(pure);
  CHECK(pure.move[0] == 1.0F);
  // Clear and computer robots write the turn directly.
  auto clear = standard(0.3F, 0.9F);
  clear.standard = false;
  apply_source_steering(clear);
  CHECK(clear.move[0] == 0.3F);
}

TEST_CASE("S.L.A.I. legs filter the mover's steer and release it at once") {
  MovementTuning tuning;
  tuning.source_controls = true;
  RobotMover mover(tuning);
  mover.place({}, 0);
  mover.tick(standard(0.3F, 0.9F), 1.0F / 60);
  CHECK(mover.rolling_steer() == 0.0F);
  CHECK(mover.heading() == 0.0F);
  mover.tick(standard(0.8F, 0.0F), 1.0F / 60);
  CHECK(mover.rolling_steer() == 0.8F);
  mover.tick(standard(0.0F, 0.0F), 1.0F / 60);
  CHECK(mover.rolling_steer() == 0.0F);
  tuning.source_controls = false;
  RobotMover native(tuning);
  native.place({}, 0);
  native.tick(standard(0.3F, 0.9F), 1.0F / 60);
  CHECK(native.rolling_steer() == 0.3F);
}

TEST_CASE("the turn motion plays faster with the turn within 0.9 and 1.1") {
  CHECK(close_to(source_turn_playback_rate(0.0F, 2.5F), 0.9F));
  // 2.5 degrees for both vsyncs is 5 degrees, 1.111 pi/40.
  const float five = source_radians_per_degree * 5.0F / source_turn_rate_step;
  CHECK(close_to(source_turn_playback_rate(1.0F, 2.5F), (1.0F + five) / 2.0F));
  CHECK(close_to(source_turn_playback_rate(-1.0F, 2.5F), (1.0F + five) / 2.0F));
  CHECK(close_to(source_turn_playback_rate(1.0F, 10.0F), 1.1F));
  MovementTuning tuning;
  tuning.turn_degrees_per_second = 120.0F;
  CHECK(close_to(source_turn_degrees(tuning), 2.0F)); // the native adapter
  tuning.source_turn_degrees = 3.0F;
  CHECK(close_to(source_turn_degrees(tuning), 3.0F));
}

TEST_CASE("the S.L.A.I. load factor grades its overload by 35-weight bands") {
  const SourceLoadRule rule;
  CHECK(source_load_factor(rule, 100.0F, 0.0F, 0.0F, 0) == 1.0F);
  CHECK(close_to(source_load_factor(rule, 100.0F, 400.0F, 500.0F, 0), 0.5F));
  CHECK(close_to(source_load_factor(rule, 100.0F, 400.0F, 500.0F, 99), 0.45F));
  CHECK(close_to(source_load_factor(rule, 100.0F, 400.0F, 380.0F, 0), 0.5F * 0.95F));
  const float ratio = 330.0F / 400.0F; // 70 over: the third band
  CHECK(
      close_to(source_load_factor(rule, 100.0F, 400.0F, 330.0F, 0), 0.5F * std::pow(ratio, 9.0F)));
  const float fourth = 295.0F / 400.0F; // 105 over: the fourth band
  CHECK(close_to(source_load_factor(rule, 100.0F, 400.0F, 295.0F, 0),
                 0.5F * std::pow(fourth, 10.0F)));
  CHECK(!parse_source_load(nullptr).has_value());
  const auto rule_json = json::parse(R"({"overload_band": 20})");
  const auto parsed = parse_source_load(&rule_json);
  REQUIRE(parsed.has_value());
  CHECK(parsed->overload_band == 20.0F);
  CHECK(parsed->graded_bands == 3);
}

TEST_CASE("S.L.A.I. legs load their turn speed by the graded factor") {
  RobotLoadout loadout;
  loadout.body = 0;
  loadout.legs = 0;
  const PartTables light = slai_tables(500.0F);
  const MovementTuning free =
      loaded_movement(light, loadout, load_stats(light, loadout), MovementTuning{});
  CHECK(free.source_controls);
  CHECK(close_to(free.source_turn_degrees, 2.0F));
  // 100 on 60 capacity: 40 over, the second band, (0.6)^4.
  const PartTables heavy = slai_tables(60.0F);
  const MovementTuning loaded =
      loaded_movement(heavy, loadout, load_stats(heavy, loadout), MovementTuning{});
  CHECK(close_to(loaded.source_turn_degrees, 2.0F * std::pow(0.6F, 4.0F)));
  PartTables native = light;
  native.legs[0].source_load.reset();
  const MovementTuning plain =
      loaded_movement(native, loadout, load_stats(native, loadout), MovementTuning{});
  CHECK(!plain.source_controls);
  CHECK(plain.source_turn_degrees == 0.0F);
}

TEST_CASE("source rolling controls default to the recovered cadence") {
  const auto node = json::parse(R"({"track": 1, "bone": 1, "source_roll": {
      "group": 0, "coefficient": 1, "pi": 3.14159, "max_step": 3.16,
      "source_units_per_native": 1, "steering_radians_per_degree": 0.0174533,
      "native_clock_rate": 60}})");
  const auto control = parse_source_roll(node);
  REQUIRE(control.has_value());
  CHECK(control->source_tick_rate == source_vsyncs_per_second);
  CHECK(control->source_frame_ticks == source_frame_vsyncs);
}
