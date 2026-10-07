#include "test_support.hpp"
#include "rumbling_fixture.hpp"
#include "../src/desktop_online_session.hpp"
#include "../src/desktop_online_presentation.hpp"
#include <chrono>
#include <thread>

using namespace phantomcrash;
namespace net = phantomcrash::online;
namespace {
struct ResultFixture {
  rumbling_fixture::Fixture data;
  TestDriveScript drive;
  std::map<std::string, WeaponTuning> weapons;
  CoopMatch match;
  ResultFixture() {
    const RumblingTables tables{data.parts, data.garage, data.tables, data.rules, drive, weapons};
    match.world = std::make_shared<RumblingWorld>(data.check(), tables, BattleSettings{},
                                                  std::array<RumblingStage, roster_areas>{},
                                                  RumblingLive{}, 7);
    match.battle = match.world->lend(0, match.simulation_save);
    match.battle->set_cooperative(true);
    match.players[1].actor = 0;
    match.battle->robot(0).piloted = true;
    match.battle->robot(0).drone = false;
    match.battle->bring_in(0);
  }
};
net::InputFrame frame(const std::uint32_t tick) {
  net::InputFrame result;
  result.tick = tick;
  result.players = 2;
  result.inputs[1].tick = tick;
  result.inputs[1].player = 1;
  return result;
}
} // namespace

TEST_CASE("online result counters and terminal outcome restore with simulation") {
  ResultFixture fixture;
  auto &match = fixture.match;
  net::MatchSimulation simulation(match, 2);
  match.players[1].won = true;
  match.players[1].victory_hold = 2 * net::tick_seconds;
  const auto before = simulation.save();
  simulation.step(frame(0));
  CHECK(!match.players[1].finished);
  simulation.step(frame(1));
  CHECK(match.players[1].finished == MatchOutcome::win);
  CHECK(match.players[1].play_seconds == 2 * double(net::tick_seconds));
  const auto finished = net::state_digest(match);
  simulation.step(frame(2));
  CHECK(match.players[1].play_seconds == 2 * double(net::tick_seconds));
  simulation.restore(*before);
  CHECK(!match.players[1].finished);
  CHECK(match.players[1].play_seconds == 0);
  simulation.step(frame(0));
  simulation.step(frame(1));
  CHECK(net::state_digest(match) == finished);
  simulation.restore(*before);
  match.players[1].won = false;
  match.battle->robot(0).alive = false;
  match.battle->robot(0).dying = true;
  simulation.step(frame(0));
  CHECK(!match.players[1].finished);
  match.battle->robot(0).dying = false;
  simulation.step(frame(1));
  CHECK(match.players[1].finished == MatchOutcome::retire);
}

TEST_CASE("predicted online finish is not published before its inputs are confirmed") {
  net::RoomOptions options;
  options.host = true;
  options.bind = net::Endpoint::parse("127.0.0.1", 0);
  net::Room host(options);
  options.host = false;
  options.server = host.local_endpoint();
  net::Room client(options);
  double now = 0;
  for (int pass = 0; pass < 40; ++pass) {
    host.update(now);
    client.update(now);
    now += 0.001;
    std::this_thread::sleep_for(std::chrono::milliseconds(1));
  }
  ResultFixture first, second;
  for (auto *fixture : {&first, &second}) {
    fixture->match.players[1].won = true;
    fixture->match.players[1].victory_hold = 2 * net::tick_seconds;
  }
  net::MatchSession local(host, first.match, 3, 2);
  net::MatchSession remote(client, second.match, 3, 2);
  CHECK(local.advance());
  CHECK(local.advance());
  CHECK(first.match.players[1].finished == MatchOutcome::win);
  const auto predicted = net::state_digest(first.match);
  CHECK(local.confirmed_finished() == 0);
  CHECK(net::state_digest(first.match) == predicted);
  RobotControl control;
  control.move = {0.25F, 0.75F};
  for (int tick = 0; tick < 2; ++tick) {
    remote.capture(control, {}, std::nullopt, now);
    CHECK(remote.advance());
    for (int pass = 0; pass < 12; ++pass) {
      host.update(now);
      client.update(now);
      for (const auto &message : host.take_messages()) {
        CHECK(local.receive(message, now));
      }
      for (const auto &message : client.take_messages()) {
        CHECK(remote.receive(message, now));
      }
      now += 0.001;
      std::this_thread::sleep_for(std::chrono::milliseconds(1));
    }
  }
  CHECK(local.confirmed_tick() == 2);
  CHECK(local.confirmed_finished() == 2);
  CHECK(local.rollback_count() > 0);
  CHECK(local.confirmed_digest(2) == remote.confirmed_digest(2));
}

TEST_CASE("online gate departure waits for its shared exit animation to finish") {
  ResultFixture fixture;
  auto &match = fixture.match;
  BattleSceneTables tables;
  tables.gates[6] = {{{0, 0, 0}, 0}};
  tables.states[scene_state::exit].frames = 3;
  SceneTuning tuning;
  tuning.entrance = tuning.computer_entrance = tuning.computer_exit = false;
  match.rider_scenes[1] = std::make_shared<BattleScene>(tables, tuning, 1);
  net::MatchSimulation simulation(match, 2);
  match.battle->place(0, {0, 0, 30}, 0);
  simulation.step(frame(0));
  match.battle->place(0, {}, 0);
  simulation.step(frame(1));
  CHECK(match.rider_scenes[1]->active());
  CHECK(!match.players[1].finished);
  simulation.step(frame(2));
  simulation.step(frame(3));
  CHECK(!match.players[1].finished);
  simulation.step(frame(4));
  CHECK(match.rider_scenes[1]->finished());
  CHECK(match.players[1].finished == MatchOutcome::leave);
}

TEST_CASE("confirmed presentation excludes corrected shots and survives a new timeline once") {
  ResultFixture fixture;
  auto &match = fixture.match;
  const auto weapons = parse_battle_settings(json::parse(R"({
    "weapons":{"gun":{"kind":"gun","damage":7,"cooldown":0.001,"speed":80,"rounds":20}}
  })"));
  match.battle->add_weapon(weapons.weapons.at("gun"));
  match.battle->arm(0, {"gun", "", "", ""});
  net::MatchSimulation simulation(match, 2);
  net::Prediction prediction(simulation, 2);
  net::Presentation presentation(0);
  auto input = frame(0).inputs[1];
  input.fire.fire[0] = true;
  prediction.submit(input);
  CHECK(prediction.advance());
  presentation.collect(match, prediction);
  for (int tick = 1; tick < 4; ++tick) {
    CHECK(prediction.advance());
  }
  presentation.collect(match, prediction);
  CHECK(presentation.pending() == 1);
  for (int tick = 1; tick < 4; ++tick) {
    input.tick = tick;
    input.fire.fire[0] = false;
    prediction.submit(input);
  }
  prediction.reconcile();
  CHECK(prediction.rollback_count() > 0);
  presentation.collect(match, prediction);
  CHECK(presentation.pending() == 4);
  std::vector<std::uint32_t> presented;
  int shots = 0;
  const auto before = net::state_digest(match);
  presentation.consume(match, [&](const auto tick) {
    presented.push_back(tick);
    for (const auto &event : match.battle->events()) {
      shots += event.robot == 0 && event.kind == BattleEventKind::fired;
    }
    ++match.players[1].bounty; // A local observer cannot alter the shared match.
  });
  CHECK(presented == std::vector<std::uint32_t>({1, 2, 3, 4}));
  CHECK(shots == 1);
  CHECK(net::state_digest(match) == before);
  presentation.collect(match, prediction);
  presentation.consume(match, [&](auto) { CHECK(false); });
  net::MatchSimulation resumed(match, 2, 4);
  net::Prediction next(resumed, 2, 4);
  input.tick = 4;
  next.submit(input);
  CHECK(next.advance());
  presentation.collect(match, next);
  const auto after = net::state_digest(match);
  CHECK_THROWS(presentation.consume(match, [&](const auto tick) {
    CHECK(tick == 5);
    ++match.players[1].bounty;
    throw std::runtime_error("presentation failure");
  }));
  CHECK(net::state_digest(match) == after);
  CHECK(presentation.pending() == 0);
}
