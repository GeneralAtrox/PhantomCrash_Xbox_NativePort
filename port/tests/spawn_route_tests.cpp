// A route over a stage's spawns for a stage without an authored one
// (spawn_route.hpp): entries away from the player, legs around walls.

#include "test_support.hpp"

#include "phantomcrash/battle.hpp"
#include "phantomcrash/json.hpp"
#include "phantomcrash/nav_graph.hpp"
#include "phantomcrash/spawn_route.hpp"

#include <algorithm>
#include <cmath>
#include <string>
#include <vector>

using namespace phantomcrash;

namespace {

// A floor from -40 to 40 and a wall 10 high over x -2..2, z -15..15, between
// the spawns at x -20 and 20.
StageCollision walled_stage() {
  return parse_stage_collision(json::parse(R"({
    "walls": [
      {"points": [[-2,0,-15],[-2,0,15],[-2,10,15]], "surface": 9},
      {"points": [[-2,0,-15],[-2,10,15],[-2,10,-15]], "surface": 9},
      {"points": [[2,0,-15],[2,10,15],[2,0,15]], "surface": 9},
      {"points": [[2,0,-15],[2,10,-15],[2,10,15]], "surface": 9},
      {"points": [[-2,0,-15],[2,10,-15],[2,0,-15]], "surface": 9},
      {"points": [[-2,0,-15],[-2,10,-15],[2,10,-15]], "surface": 9},
      {"points": [[-2,0,15],[2,0,15],[2,10,15]], "surface": 9},
      {"points": [[-2,0,15],[2,10,15],[-2,10,15]], "surface": 9},
      {"points": [[-2,10,-15],[2,10,15],[2,10,-15]], "surface": 9},
      {"points": [[-2,10,-15],[-2,10,15],[2,10,15]], "surface": 9}
    ],
    "floor": [
      {"points": [[-40,0,-40],[40,0,40],[40,0,-40]], "surface": 9},
      {"points": [[-40,0,-40],[-40,0,40],[40,0,40]], "surface": 9}
    ]
  })"));
}

NavTuning tuning() {
  NavTuning t;
  t.background = false;
  return t;
}

constexpr Vec3 west{-20.0F, 0.0F, 0.0F};
constexpr Vec3 east{20.0F, 0.0F, 0.0F};
constexpr Vec3 north{0.0F, 0.0F, 30.0F};
constexpr float merge = 3.0F;

bool near(const Vec3 a, const Vec3 b) {
  return std::hypot(a.x - b.x, a.z - b.z) < 0.5F;
}

// Whether the drone walking `path` on the walled stage ever takes segment 1
// within ten seconds.
bool drone_moves_on(const BattleSettings &settings, const EnemyPath &path) {
  Battle battle(settings, path);
  battle.set_stage(walled_stage());
  battle.add_robot("player", 0, {0.0F, 0.0F, -38.0F}, 0.0F, {}, 100.0F);
  const int drone = battle.add_drone("drone", 100.0F);
  constexpr float tick = 1.0F / 60.0F;
  for (int i = 0; i < 600; ++i) {
    battle.tick({}, {}, tick);
    if (battle.robots()[static_cast<std::size_t>(drone)].path.segment() == 1) {
      return true;
    }
  }
  return false;
}

const EnemyPath::Segment *leg(const EnemyPath &path, const Vec3 from, const Vec3 to) {
  for (const EnemyPath::Segment &segment : path.segments) {
    if (segment.kind != 0 && near(segment.points.front(), from) &&
        near(segment.points.back(), to)) {
      return &segment;
    }
  }
  return nullptr;
}

} // namespace

TEST_CASE("drones enter at the spawns away from the player and roam all of them") {
  const StageCollision stage = walled_stage();
  const NavGraph graph = NavGraph::build(stage, tuning());
  const std::vector<Vec3> spawns{west, east, north, west};
  const EnemyPath path = spawn_route(stage, graph, tuning(), spawns, north, merge);
  // Three places (the second west is the first), six legs, four entries.
  REQUIRE(path.segments.size() == 10);
  const std::vector<int> entries = path.entries();
  REQUIRE(entries.size() == 4);
  for (const int entry : entries) {
    CHECK(!near(path.segments[static_cast<std::size_t>(entry)].points.front(), north));
  }
  // A clear walk is one straight line.
  const EnemyPath::Segment *open = leg(path, west, north);
  REQUIRE(open != nullptr);
  CHECK(open->points.size() == 2);
  // From west to east goes on north or back west, but not straight back
  // while north is joined.
  const EnemyPath::Segment *across = leg(path, west, east);
  REQUIRE(across != nullptr);
  REQUIRE(across->next.size() == 1);
  CHECK(near(path.segments[static_cast<std::size_t>(across->next[0])].points.back(), north));
}

TEST_CASE("a leg goes round the wall in its way") {
  const StageCollision stage = walled_stage();
  const NavGraph graph = NavGraph::build(stage, tuning());
  const std::vector<Vec3> spawns{west, east};
  const EnemyPath path = spawn_route(stage, graph, tuning(), spawns, north, merge);
  const EnemyPath::Segment *across = leg(path, west, east);
  REQUIRE(across != nullptr);
  CHECK(across->points.size() > 2);
  CHECK(std::ranges::any_of(across->points, [](const Vec3 p) { return std::abs(p.z) > 15.0F; }));
  // Every step is walkable as the grid's own walk links are.
  for (std::size_t i = 1; i < across->points.size(); ++i) {
    CHECK(nav_walkable(stage, across->points[i - 1], across->points[i], tuning().step, 0.0F));
  }
  // Neither spawn is the player's, so both legs are entries too.
  CHECK(path.entries().size() == 2);
}

TEST_CASE("one place makes no route") {
  const StageCollision stage = walled_stage();
  const NavGraph graph = NavGraph::build(stage, tuning());
  const std::vector<Vec3> spawns{west, {west.x + 1.0F, 0.0F, 0.0F}};
  CHECK(spawn_route(stage, graph, tuning(), spawns, north, merge).segments.empty());
}

TEST_CASE("walking drones keep to the route they can walk") {
  const StageCollision stage = walled_stage();
  const NavGraph graph = NavGraph::build(stage, tuning());
  constexpr Vec3 south{0.0F, 0.0F, -30.0F};
  constexpr Vec3 far_north{0.0F, 0.0F, 38.0F};
  EnemyPath path;
  // 0 comes in at the west; 1 climbs the wall; 2 goes round it; 3 walks
  // through it; 4 leads nowhere, so the grid walks it back to 2.
  path.segments.push_back({0, {west, north}, {1, 2, 3, 4}});
  path.segments.push_back({6, {north, {0.0F, 10.0F, 0.0F}, east}, {2}});
  path.segments.push_back({1, {north, east, south, west}, {0}});
  path.segments.push_back({1, {north, west, east}, {2}});
  path.segments.push_back({1, {north, far_north}, {}});
  const EnemyPath walked = walker_route(stage, graph, tuning(), path, west);
  REQUIRE(walked.segments.size() == 4);
  CHECK(walked.segments[0].kind == 0);
  CHECK((walked.segments[0].next == std::vector<int>{1, 2}));
  CHECK(walked.segments[1].next == std::vector<int>{0});
  CHECK(walked.segments[2].next == std::vector<int>{3});
  const EnemyPath::Segment &join = walked.segments[3];
  CHECK(join.next == std::vector<int>{1});
  CHECK(near(join.points.front(), far_north));
  CHECK(near(join.points.back(), north));
  CHECK(walker_route(stage, graph, tuning(), {}, west).segments.empty());
}

TEST_CASE("a stand-in drone held by a wall hops, then takes a next segment") {
  // Segment 0 runs through the wall; segment 1 goes round to its start.
  EnemyPath path;
  path.segments.push_back({0, {west, east}, {1}});
  path.segments.push_back({1, {{-20.0F, 0.0F, 30.0F}, west}, {0}});
  BattleSettings settings;
  CHECK(!drone_moves_on(settings, path));
  settings.drone_unstick_seconds = 0.5F;
  settings.drone_hop_seconds = 0.1F;
  settings.drone_hops = 1;
  CHECK(drone_moves_on(settings, path));
}
