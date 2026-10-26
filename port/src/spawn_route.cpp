#include "phantomcrash/spawn_route.hpp"

#include <algorithm>
#include <cmath>
#include <cstddef>
#include <limits>
#include <optional>
#include <vector>

namespace phantomcrash {
namespace {

constexpr int entry_kind = 0; // drones come in here (EnemyPath::entries)
constexpr int roam_kind = 1;  // a plain route segment, walked at full pace
// No jump climbs, and no jump across a gap either: every jump link is left out.
constexpr float no_jumps = -std::numeric_limits<float>::infinity();
// A route segment left leading nowhere is joined to the first of this many
// nearest segments the grid walks it to.
constexpr std::size_t join_tries = 3;

float flat_distance(const Vec3 a, const Vec3 b) {
  return length(Vec3{a.x - b.x, 0.0F, a.z - b.z});
}

// Each point kept is the farthest a straight walk from the last one reaches.
std::vector<Vec3> straightened(const StageCollision &stage, const NavTuning &tuning,
                               const std::vector<Vec3> &points) {
  std::vector<Vec3> out{points.front()};
  std::size_t at = 0;
  while (at + 1 < points.size()) {
    std::size_t far = at + 1;
    while (far + 1 < points.size() &&
           nav_walkable(stage, points[at], points[far + 1], tuning.step, tuning.wall_clearance)) {
      ++far;
    }
    out.push_back(points[far]);
    at = far;
  }
  return out;
}

// The walk from `from` to `to`, or nothing when the grid joins them only by
// jumps or not at all.
std::optional<std::vector<Vec3>> leg(const StageCollision &stage, const NavGraph &graph,
                                     const NavTuning &tuning, const Vec3 from, const Vec3 to) {
  const int start = graph.nearest(from);
  const int goal = graph.nearest(to);
  if (start < 0 || goal < 0) {
    return std::nullopt;
  }
  std::vector<Vec3> points{from};
  if (start != goal) {
    const std::vector<NavWaypoint> route = graph.path(start, goal, {}, 0.0F, no_jumps);
    if (route.empty()) {
      return std::nullopt;
    }
    for (const NavWaypoint &waypoint : route) {
      points.push_back(waypoint.position);
    }
  }
  points.push_back(to);
  return straightened(stage, tuning, points);
}

} // namespace

EnemyPath spawn_route(const StageCollision &stage, const NavGraph &graph, const NavTuning &tuning,
                      const std::span<const Vec3> spawns, const Vec3 player_start,
                      const float merge) {
  std::vector<Vec3> places;
  for (const Vec3 spawn : spawns) {
    if (std::ranges::none_of(places,
                             [&](const Vec3 p) { return flat_distance(p, spawn) < merge; })) {
      places.push_back(spawn);
    }
  }
  const std::size_t count = places.size();
  // legs[from * count + to]: the roaming segment between two places, or -1.
  std::vector<int> legs(count * count, -1);
  EnemyPath path;
  for (std::size_t from = 0; from < count; ++from) {
    for (std::size_t to = 0; to < count; ++to) {
      if (from == to) {
        continue;
      }
      if (auto points = leg(stage, graph, tuning, places[from], places[to])) {
        legs[from * count + to] = static_cast<int>(path.segments.size());
        path.segments.push_back({roam_kind, std::move(*points), {}});
      }
    }
  }
  if (path.segments.empty()) {
    return {};
  }
  // On from a place to any other, not straight back while another is joined.
  const auto onward = [&](const std::size_t from, const std::size_t at) {
    std::vector<int> next, back;
    for (std::size_t to = 0; to < count; ++to) {
      const int segment = legs[at * count + to];
      if (segment >= 0) {
        (to == from ? back : next).push_back(segment);
      }
    }
    return next.empty() ? back : next;
  };
  for (std::size_t from = 0; from < count; ++from) {
    for (std::size_t to = 0; to < count; ++to) {
      if (const int segment = legs[from * count + to]; segment >= 0) {
        path.segments[static_cast<std::size_t>(segment)].next = onward(from, to);
      }
    }
  }
  // The entries: each leg out of a place the player does not start at.
  const std::vector<EnemyPath::Segment> roaming = path.segments;
  for (std::size_t from = 0; from < count; ++from) {
    if (flat_distance(places[from], player_start) < merge) {
      continue;
    }
    for (std::size_t to = 0; to < count; ++to) {
      if (const int segment = legs[from * count + to]; segment >= 0) {
        EnemyPath::Segment entry = roaming[static_cast<std::size_t>(segment)];
        entry.kind = entry_kind;
        path.segments.push_back(std::move(entry));
      }
    }
  }
  return path;
}

EnemyPath walker_route(const StageCollision &stage, const NavGraph &graph, const NavTuning &tuning,
                       const EnemyPath &path, const Vec3 from) {
  const int start = graph.nearest(from);
  if (start < 0) {
    return {};
  }
  const std::vector<bool> reach = graph.reachable_from(start, no_jumps);
  // Where a drone at the route point stands: on the ground by the grid node
  // nearest it, when walks and drops reach that node.
  const auto standing = [&](const Vec3 p) -> std::optional<Vec3> {
    const int node = graph.nearest(p);
    if (node < 0 || !reach[static_cast<std::size_t>(node)]) {
      return std::nullopt;
    }
    const float near = graph.nodes()[static_cast<std::size_t>(node)].position.y;
    const float ground = stage.ground_height({p.x, near, p.z}, tuning.step);
    if (std::abs(near - p.y) > tuning.point_rise || ground == no_ground) {
      return std::nullopt;
    }
    return Vec3{p.x, ground, p.z};
  };
  // Drones walk straight from point to point, as the grid's own walks.
  const auto walk = [&](const Vec3 a, const Vec3 b) {
    const std::optional<Vec3> at = standing(a);
    const std::optional<Vec3> to = standing(b);
    return at && to && nav_walkable(stage, *at, *to, tuning.step, 0.0F);
  };
  const std::size_t count = path.segments.size();
  std::vector<bool> open(count, false);
  for (std::size_t i = 0; i < count; ++i) {
    const std::vector<Vec3> &points = path.segments[i].points;
    bool walked = !points.empty() && standing(points.front());
    for (std::size_t k = 1; walked && k < points.size(); ++k) {
      walked = walk(points[k - 1], points[k]);
    }
    open[i] = walked;
  }
  std::vector<std::vector<int>> next(count);
  for (std::size_t i = 0; i < count; ++i) {
    for (const int link : open[i] ? path.segments[i].next : std::vector<int>{}) {
      const auto &to = path.segments[static_cast<std::size_t>(link)];
      if (open[static_cast<std::size_t>(link)] &&
          walk(path.segments[i].points.back(), to.points.front())) {
        next[i].push_back(link);
      }
    }
  }
  // A dead end would hold a drone pacing one segment: the grid walks it on
  // to one of the nearest segments left. One it cannot is left out, until
  // every segment left leads on.
  std::vector<EnemyPath::Segment> segments = path.segments;
  for (std::size_t i = 0; i < count; ++i) {
    if (!open[i] || !next[i].empty()) {
      continue;
    }
    const Vec3 end = segments[i].points.back();
    std::vector<int> nearest;
    for (std::size_t j = 0; j < count; ++j) {
      if (open[j] && j != i) {
        nearest.push_back(static_cast<int>(j));
      }
    }
    std::ranges::sort(nearest, {}, [&](const int j) {
      return flat_distance(end, segments[static_cast<std::size_t>(j)].points.front());
    });
    nearest.resize(std::min(nearest.size(), join_tries));
    for (const int j : nearest) {
      const Vec3 start = segments[static_cast<std::size_t>(j)].points.front();
      if (std::optional<std::vector<Vec3>> walk = leg(stage, graph, tuning, end, start)) {
        next[i].push_back(static_cast<int>(segments.size()));
        segments.push_back({roam_kind, std::move(*walk), {j}});
        open.push_back(true);
        next.push_back({j});
        break;
      }
    }
  }
  const std::size_t total = segments.size();
  const auto leads_on = [&](const int link) { return open[static_cast<std::size_t>(link)]; };
  for (bool changed = true; changed;) {
    changed = false;
    for (std::size_t i = 0; i < total; ++i) {
      if (open[i] && std::ranges::none_of(next[i], leads_on)) {
        open[i] = false;
        changed = true;
      }
    }
  }
  // kept[i]: segment i's index in the result, or -1.
  std::vector<int> kept(total, -1);
  EnemyPath out;
  for (std::size_t i = 0; i < total; ++i) {
    if (open[i]) {
      kept[i] = static_cast<int>(out.segments.size());
      out.segments.push_back(segments[i]);
      out.segments.back().next.clear();
    }
  }
  for (std::size_t i = 0; i < total; ++i) {
    for (const int link : open[i] ? next[i] : std::vector<int>{}) {
      if (const int to = kept[static_cast<std::size_t>(link)]; to >= 0) {
        out.segments[static_cast<std::size_t>(kept[i])].next.push_back(to);
      }
    }
  }
  return out;
}

} // namespace phantomcrash
