// Port navigation for computer robots: following planned routes over the
// stage's navigation grid, with held jumps up ledges, and taking high
// ground in a fight (nav_graph.hpp; rumbling.json live.ai.navigation).
#include "phantomcrash/battle.hpp"

#include <algorithm>
#include <chrono>
#include <cmath>
#include <limits>

namespace phantomcrash {
namespace {

constexpr float facing_jump = 0.35F;      // radians: a jump link is taken facing this close
constexpr float facing_walk = 1.0F;       // walk on while the waypoint is this close ahead
constexpr float lift_margin = 1.0F;       // let go this far under the landing; the throw clears it
constexpr float hop_rise = 3.5F;          // a stuck robot's hop holds the jump this far up
constexpr float progress_epsilon = 0.25F; // nearer by this much counts as progress
constexpr float level_slack = 0.5F;       // a waypoint is reached within a step plus this
constexpr float goal_snap_cells = 1.5F;   // a goal this near its node is walked to exactly
constexpr float vantage_rise_weight = 1.5F; // each unit higher is worth this much path
constexpr float overshoot = 0.5F;           // in the air, stop pushing on this close
constexpr int smooth_lookahead = 8;         // waypoints a straight walk may skip at once

float flat(Vec3 v) {
  v.y = 0;
  return length(v);
}

float stick(const float error, const float speed, const float seconds) {
  return speed > 0 ? std::clamp(error / (speed * seconds), -1.0F, 1.0F) : 0;
}

// Drops walk waypoints that a straight walk can skip.
std::vector<NavWaypoint> smooth(const StageCollision &stage, const std::vector<NavWaypoint> &route,
                                Vec3 from, const float step, const float half_width) {
  std::vector<NavWaypoint> kept;
  std::size_t i = 0;
  while (i < route.size()) {
    std::size_t reach = i;
    if (route[i].link == NavLink::walk) {
      const std::size_t last = std::min(route.size() - 1, i + smooth_lookahead);
      for (std::size_t j = last; j > i; --j) {
        const bool walks =
            std::all_of(route.begin() + static_cast<std::ptrdiff_t>(i) + 1,
                        route.begin() + static_cast<std::ptrdiff_t>(j) + 1,
                        [](const NavWaypoint &w) { return w.link == NavLink::walk; });
        if (walks && nav_walkable(stage, from, route[j].position, step, half_width)) {
          reach = j;
          break;
        }
      }
    }
    kept.push_back(route[reach]);
    from = route[reach].position;
    i = reach + 1;
  }
  return kept;
}

} // namespace

void Battle::set_stage(StageCollision stage) {
  stage_ = std::move(stage);
  nav_.reset();
  nav_future_ = settings_.ai.navigation.enabled ? shared_nav_graph(stage_, settings_.ai.navigation)
                                                : NavGraphFuture{};
}

const NavGraph *Battle::nav_graph() {
  if (!nav_ && nav_future_.valid() &&
      nav_future_.wait_for(std::chrono::seconds(0)) == std::future_status::ready) {
    nav_ = nav_future_.get();
  }
  return nav_.get();
}

bool Battle::ai_plan(Combatant &drone, const Vec3 goal) {
  const auto &t = settings_.ai.navigation;
  const NavGraph &graph = *nav_;
  NavAgent &n = drone.ai.nav;
  const Vec3 at = drone.mover.position();
  const int start = graph.nearest(at);
  const int goal_node = graph.nearest(goal);
  if (start >= 0 && goal_node >= 0 && start != goal_node) {
    if (!take_nav_search()) {
      return true; // this tick's searches are spent: keep the route, plan next tick
    }
  }
  n.route.clear();
  n.next = 0;
  n.stall = 0;
  n.best = std::numeric_limits<float>::max();
  n.replan_left = t.replan_seconds;
  n.since_plan = 0;
  n.goal = goal;
  n.goal_node = goal_node;
  if (start < 0 || n.goal_node < 0) {
    return false;
  }
  if (start != n.goal_node) {
    n.route = graph.path(start, n.goal_node, n.avoided, t.avoid_cost);
    if (n.route.empty()) {
      return false;
    }
  }
  // Finish at the goal itself, at its node's height: ENPT points can lie
  // a little under the street.
  const Vec3 end = graph.nodes()[static_cast<std::size_t>(n.goal_node)].position;
  if (flat(goal - end) < t.cell * goal_snap_cells) {
    n.route.push_back({{goal.x, end.y, goal.z}, NavLink::walk, n.goal_node});
  }
  if (!n.rough) {
    n.route = smooth(stage_, n.route, at, t.step, drone.radius * 0.8F);
  }
  return true;
}

bool Battle::ai_navigate(Combatant &drone, const Vec3 goal, const float seconds,
                         const float speed) {
  const auto &t = settings_.ai.navigation;
  if (!t.enabled || nav_graph() == nullptr) {
    return false;
  }
  auto &s = drone.ai;
  NavAgent &n = s.nav;
  for (NavAvoid &a : n.avoided) {
    a.seconds -= seconds;
  }
  std::erase_if(n.avoided, [](const NavAvoid &a) { return a.seconds <= 0; });
  const Vec3 at = drone.mover.position();
  const bool grounded = drone.mover.grounded();
  n.replan_left -= seconds;
  n.since_plan += seconds;
  n.air_seconds = grounded ? 0.0F : n.air_seconds + seconds;
  // Wedged in the air on geometry, it plans from where it hangs.
  const bool settled = grounded || n.air_seconds > t.give_up_seconds;
  const bool arrived = !n.route.empty() && n.next >= n.route.size();
  const bool moved = length(goal - n.goal) > t.goal_moved && n.since_plan >= t.replan_min_seconds;
  if (settled && (!n.lifting || !grounded) &&
      (moved || n.route.empty() || (n.replan_left <= 0 && !arrived))) {
    if (!ai_plan(drone, goal)) {
      return false;
    }
  }
  while (n.next < n.route.size()) {
    const NavWaypoint &w = n.route[n.next];
    if (!grounded || flat(w.position - at) >= t.waypoint_reach ||
        std::abs(w.position.y - at.y) > t.step + level_slack) {
      break;
    }
    ++n.next;
    n.stall = 0;
    n.best = std::numeric_limits<float>::max();
  }
  n.active = true;
  // Past the route's end, the rest of the way is walked straight.
  const bool finished = n.next >= n.route.size();
  if (finished && flat(goal - at) <= t.waypoint_reach) {
    s.control.move[1] = 0;
    return true;
  }
  const NavWaypoint w =
      finished ? NavWaypoint{{goal.x, at.y, goal.z}, NavLink::walk, -1} : n.route[n.next];
  const Vec3 to = w.position - at;
  const bool jump = w.link == NavLink::jump;
  const float error = wrap_angle(heading_toward(to) - drone.mover.heading());
  s.control.move[0] = stick(error, radians(drone.mover.tuning().turn_degrees_per_second), seconds);
  const auto press = [&](const float lift_to) {
    // A press needs the jump released first: alternate while it has not
    // taken (the landing ignores the jump for a moment).
    s.control.jump = !n.pressed;
    n.pressed = !n.pressed;
    n.lifting = true;
    n.lift_to = lift_to;
  };
  if (grounded) {
    n.lifting = false;
    if (jump) {
      const bool facing = std::abs(error) < facing_jump;
      s.control.move[1] = facing ? 1.0F : 0.0F;
      if (facing) {
        press(w.position.y - lift_margin);
      }
    } else {
      n.pressed = false;
      s.control.move[1] = std::abs(error) < facing_walk ? speed : 0.0F;
    }
  } else {
    n.pressed = false;
    s.control.move[1] = flat(to) > overshoot ? 1.0F : 0.0F;
    if (n.lifting && at.y < n.lift_to) {
      s.control.jump = true;
    } else {
      n.lifting = false; // let go once; pressing again would dive
    }
  }
  const float distance = flat(to) + (jump ? std::max(0.0F, w.position.y - at.y) : 0.0F);
  if (distance < n.best - progress_epsilon) {
    n.best = distance;
    n.stall = 0;
  } else {
    n.stall += seconds;
  }
  if (grounded && !jump && n.stall > t.stuck_seconds) {
    press(at.y + hop_rise);
  }
  if (n.stall > t.give_up_seconds) {
    // First plan again without shortcuts; stuck again, avoid the waypoint.
    if (n.rough && w.node >= 0) {
      n.avoided.push_back({-1, w.node, t.avoid_seconds});
    }
    n.rough = !n.rough;
    n.replan_left = 0;
    n.route.clear();
    n.stall = 0;
    n.best = std::numeric_limits<float>::max();
    n.lifting = false;
    ++s.recoveries;
  }
  return true;
}

std::vector<bool> Battle::nav_reach(const Combatant &drone) {
  std::vector<bool> reach;
  if (!settings_.ai.navigation.enabled || nav_graph() == nullptr) {
    return reach;
  }
  return nav_->reachable_from(nav_->nearest(drone.mover.position()));
}

bool Battle::take_nav_search() {
  if (nav_plans_left_ <= 0) {
    return false;
  }
  --nav_plans_left_;
  return true;
}

bool Battle::nav_reaches(const std::vector<bool> &reach, const Vec3 point) const {
  if (reach.empty()) {
    return true;
  }
  const int node = nav_->nearest(point);
  return node >= 0 && reach[static_cast<std::size_t>(node)];
}

int Battle::ai_vantage(const Combatant &drone) {
  const auto &t = settings_.ai.navigation;
  const auto &s = drone.ai;
  const NavGraph &graph = *nav_;
  const Combatant &enemy = robots_.at(static_cast<std::size_t>(s.target));
  float range = 0;
  for (std::size_t slot = 0; slot < weapon_slots; ++slot) {
    const WeaponTuning *weapon = drone.weapons[slot];
    if (weapon && !drone.lost[slot] && weapon->kind != WeaponKind::melee) {
      range = std::max(range, weapon->range);
    }
  }
  const float reach = std::min(t.vantage_radius, range * t.vantage_range_share);
  if (reach <= t.vantage_nearest) {
    return -1;
  }
  const Vec3 feet = enemy.mover.position();
  const Vec3 aim = enemy.aim_origin();
  std::vector<std::pair<float, int>> candidates;
  for (const auto &[node, cost] :
       graph.costs_from(graph.nearest(drone.mover.position()), t.vantage_path_limit)) {
    const Vec3 p = graph.nodes()[static_cast<std::size_t>(node)].position;
    const float rise = p.y - feet.y;
    const float distance = flat(p - feet);
    if (rise >= t.vantage_rise && distance >= t.vantage_nearest && distance <= reach) {
      candidates.emplace_back(cost - std::min(rise, t.jump_rise) * vantage_rise_weight, node);
    }
  }
  std::ranges::sort(candidates);
  const auto tests = std::min(candidates.size(), static_cast<std::size_t>(t.vantage_sight_tests));
  for (std::size_t k = 0; k < tests; ++k) {
    const int node = candidates[k].second;
    const Vec3 eye =
        graph.nodes()[static_cast<std::size_t>(node)].position + Vec3{0.0F, drone.radius, 0.0F};
    if (clear_target_sight(eye, aim)) {
      return node;
    }
  }
  return -1;
}

bool Battle::ai_high_ground(Combatant &drone, const float seconds) {
  const auto &t = settings_.ai.navigation;
  auto &s = drone.ai;
  NavAgent &n = s.nav;
  n.holding = false;
  if (!t.enabled || !t.high_ground || s.target < 0 || nav_graph() == nullptr) {
    return false;
  }
  if (n.vantage_target != s.target) {
    n.vantage_target = s.target;
    n.vantage = -1;
    n.vantage_left = 0;
  }
  n.vantage_left -= seconds;
  // The search waits for a tick with a graph search to spare.
  if (n.vantage_left <= 0 && drone.mover.grounded() && take_nav_search()) {
    n.vantage_left = t.vantage_seconds;
    n.vantage = ai_random(random_) < t.vantage_chance ? ai_vantage(drone) : -1;
  }
  if (n.vantage < 0) {
    return false;
  }
  const Vec3 spot = nav_->nodes()[static_cast<std::size_t>(n.vantage)].position;
  const Vec3 at = drone.mover.position();
  if (flat(spot - at) < t.waypoint_reach * 2.0F && std::abs(spot.y - at.y) < t.step + level_slack) {
    n.holding = true;
    return false; // holding it: fight as usual, without closing in
  }
  if (!ai_navigate(drone, spot, seconds, 1.0F)) {
    n.vantage = -1;
    return false;
  }
  return true;
}

} // namespace phantomcrash
