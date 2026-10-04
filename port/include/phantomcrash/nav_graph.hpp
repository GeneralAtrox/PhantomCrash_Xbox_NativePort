#pragma once

#include "phantomcrash/json.hpp"
#include "phantomcrash/math3d.hpp"
#include "phantomcrash/stage_collision.hpp"

#include <cstdint>
#include <future>
#include <memory>
#include <span>
#include <utility>
#include <vector>

// Port navigation for computer robots (a port addition; the original only
// follows its authored ENPT polylines). A grid over the stage's standable
// ground, one node per cell and level, linked by walks, held jumps up ledges
// and across gaps, and drops. Off by default (rumbling.json live.ai.navigation).
namespace phantomcrash {

struct NavTuning {
  bool enabled{};
  bool background{true}; // build on a worker thread (tests build at once)
  float cell{2.0F};      // grid spacing, units
  float step{0.9F};      // the highest rise walked between neighbouring cells
  // The highest ledge a planned jump climbs; a held jump rises about 16
  // (movement.jump_height), so this leaves room to clear the lip.
  float jump_rise{12.0F};
  int jump_cells{3};              // how far across a jump or drop reaches, in cells
  float headroom{4.4F};           // clear height a node needs (the robot's height)
  float wall_clearance{1.6F};     // nodes keep about this far from walls
  float jump_cost{6.0F};          // extra cost of a jump link, in units walked
  float drop_cost{2.0F};          // extra cost of a drop link
  float waypoint_reach{1.6F};     // a waypoint counts as reached this close
  float replan_seconds{2.0F};     // the plan is refreshed this often
  float stuck_seconds{1.0F};      // no progress this long: hop
  float give_up_seconds{2.5F};    // ... this long: avoid the link and plan again
  float avoid_seconds{10.0F};     // how long an avoided link stays avoided
  float avoid_cost{250.0F};       // what an avoided link costs the planner
  float goal_moved{4.0F};         // the goal moving this far plans again
  float replan_min_seconds{0.5F}; // ... but no sooner than this after the last plan
  // Searches for all robots in one battle tick (0: no limit); a robot over
  // the limit keeps its route and plans in a later tick. A search costs up
  // to a few milliseconds on the big stages.
  int plans_per_tick{2};
  // High ground: during a fight, take a vantage above the target.
  bool high_ground{};
  float vantage_rise{4.0F};        // at least this far above the target's feet
  float vantage_radius{45.0F};     // searched this far around the target
  float vantage_nearest{10.0F};    // and no closer to it than this
  float vantage_range_share{0.8F}; // within this share of the weapon's range
  float vantage_path_limit{90.0F}; // the longest path worth taking to one
  float vantage_seconds{8.0F};     // a vantage is held this long before choosing again
  float vantage_chance{0.6F};      // the share of choices that look for one
  int vantage_sight_tests{48};     // sight lines tested per choice
};

[[nodiscard]] NavTuning parse_nav_tuning(const json::JsonValue &value);

enum class NavLink : std::uint8_t { walk, jump, drop };

struct NavEdge {
  std::uint32_t to{};
  float cost{};
  NavLink link{};
};

struct NavNode {
  Vec3 position;                  // the robot's feet
  std::uint32_t first{}, count{}; // its outgoing edges
  bool ledge{}; // a neighbouring cell has no walkable node: a lip or a wall's foot
};

// A point on a planned route and how it is reached from the one before.
struct NavWaypoint {
  Vec3 position;
  NavLink link{};
  int node{-1};
};

struct NavAvoid {
  int from{-1}, to{-1};
  float seconds{};
};

class NavGraph final {
public:
  [[nodiscard]] static NavGraph build(const StageCollision &stage, const NavTuning &tuning);

  // The node a robot standing (or about to land) at `at` is on: the
  // nearest in the cell and its neighbours, preferring its own level.
  [[nodiscard]] int nearest(Vec3 at) const;
  // A* over the links; avoided links cost `avoid_cost` more. The route
  // leaves out `from` and ends at `to`; empty when there is none.
  [[nodiscard]] std::vector<NavWaypoint> path(int from, int to, std::span<const NavAvoid> avoided,
                                              float avoid_cost) const;
  // Path costs from `from` to every node no costlier than `limit`.
  [[nodiscard]] std::vector<std::pair<int, float>> costs_from(int from, float limit) const;
  // Whether each node can be reached from `from` at all: a flood without
  // costs, far cheaper than costs_from over the whole graph. Empty when
  // `from` is no node.
  [[nodiscard]] std::vector<bool> reachable_from(int from) const;
  // Nodes whose cells lie within `radius` of (x, z).
  [[nodiscard]] std::vector<int> nodes_near(float x, float z, float radius) const;

  [[nodiscard]] const std::vector<NavNode> &nodes() const noexcept {
    return nodes_;
  }
  [[nodiscard]] const std::vector<NavEdge> &edges() const noexcept {
    return edges_;
  }
  [[nodiscard]] std::span<const NavEdge> edges_of(int node) const {
    const NavNode &n = nodes_[static_cast<std::size_t>(node)];
    return {edges_.data() + n.first, n.count};
  }

private:
  friend class NavGraphBuilder;
  [[nodiscard]] int cell_of(float x, float z) const;

  float origin_x_{}, origin_z_{}, cell_{2.0F};
  int columns_{}, rows_{};
  std::vector<std::uint32_t> cell_first_; // nodes of cell c: [cell_first_[c], cell_first_[c + 1])
  std::vector<NavNode> nodes_;
  std::vector<NavEdge> edges_;
};

// A straight walk from `from` to `to` stays on ground within `step` of the
// line and meets no wall at knee or chest height, nor at knee height
// `half_width` to either side.
[[nodiscard]] bool nav_walkable(const StageCollision &stage, Vec3 from, Vec3 to, float step,
                                float half_width);

// One graph per stage and tuning, shared by every battle on that stage.
// With `background`, it is built on a worker thread; poll the future.
using NavGraphFuture = std::shared_future<std::shared_ptr<const NavGraph>>;
[[nodiscard]] NavGraphFuture shared_nav_graph(const StageCollision &stage, const NavTuning &tuning);

// A robot's progress along its planned route (kept in RobotAiState).
struct NavAgent {
  std::vector<NavWaypoint> route;
  std::size_t next{};
  int goal_node{-1};
  Vec3 goal;
  float replan_left{};
  float since_plan{};  // seconds since the last plan
  float stall{};       // seconds without getting nearer the next waypoint
  float best{};        // the nearest it has come to the next waypoint
  bool lifting{};      // holding the jump of a jump link (or a hop)
  float lift_to{};     // ... until the feet are this high
  bool active{};       // following a route this AI tick
  bool rough{};        // plan without shortcuts (after getting stuck on one)
  bool pressed{};      // the jump was pressed last AI tick on the ground
  float air_seconds{}; // how long it has been off the ground
  std::vector<NavAvoid> avoided;
  int vantage{-1}; // the node taken as high ground, or -1
  bool holding{};  // standing on it this AI tick
  int vantage_target{-1};
  float vantage_left{};
};

} // namespace phantomcrash
