#pragma once

#include "phantomcrash/enemy_path.hpp"
#include "phantomcrash/nav_graph.hpp"

#include <span>

namespace phantomcrash {

// A route for a stage without an authored one (an imported stage without
// its game's route; a port addition): drones come in at every spawn but the player's and roam from
// spawn to spawn. Each leg is planned over the stage's navigation grid by
// walks and drops only, as a patrolling drone walks and cannot jump, then
// straightened wherever a straight walk keeps the grid's wall clearance.
// Spawns closer than `merge` are one; a spawn within `merge` of
// `player_start` is roamed to but never entered at. Empty when no two spawns
// are joined.
[[nodiscard]] EnemyPath spawn_route(const StageCollision &stage, const NavGraph &graph,
                                    const NavTuning &tuning, std::span<const Vec3> spawns,
                                    Vec3 player_start, float merge);

// The part of `path` a drone that walks straight from point to point and
// never jumps can follow from `from` (port choice for drones without the
// robots' AI). A segment is left out when a point is off the ground the grid
// walks to from `from` (no node within `tuning.point_rise`, one reached only
// by jumps, or no ground there) or a straight walk between its points meets
// a wall, a hole, a drop or a step up, as the grid tests its own walks; so is
// a link that does. A segment then
// leading nowhere gets a walk planned over the grid to one of the nearest
// segments left, or is left out too. Empty when nothing is left.
[[nodiscard]] EnemyPath walker_route(const StageCollision &stage, const NavGraph &graph,
                                     const NavTuning &tuning, const EnemyPath &path, Vec3 from);

} // namespace phantomcrash
