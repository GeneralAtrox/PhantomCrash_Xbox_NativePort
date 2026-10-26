#pragma once

// Per-part appearance (a port addition, data/ui/part_looks.json): a part in
// the stock can wear the mesh of another part of its kind. Only the drawn
// robot changes; the part's stats, and the motions, timings and size the
// rules read, stay its own (StockItem::look).

#include "phantomcrash/garage.hpp"
#include "phantomcrash/json.hpp"
#include "phantomcrash/robot.hpp"
#include "phantomcrash/robot_parts.hpp"
#include "phantomcrash/sprite_layer.hpp"

#include <array>
#include <optional>
#include <vector>

namespace phantomcrash {

struct PartLookRules {
  bool enabled{true};
  // Weapons keep to their own weapon kind (a blade wears blades), and arm
  // weapons to their arm class, so their poses and muzzles still fit.
  bool weapons_keep_kind{true};
  bool arms_keep_class{true};
};

// Throws std::runtime_error naming the entry at fault.
[[nodiscard]] PartLookRules parse_part_look_rules(const json::JsonValue &document);

// The arrows either side of the robot that left and right (or a click)
// turn a part's look with: left, then right.
using LookArrows = std::array<Sprite, 2>;
// A screen's "look_arrows"; none without (no looks there).
[[nodiscard]] std::optional<LookArrows> parse_look_arrows(const json::JsonValue &screen);

class PartLooks {
public:
  PartLooks() = default;
  PartLooks(const RobotCatalogue &catalogue, const PartTables &parts, const PartLookRules &rules);

  // The parts whose meshes part `id` of `kind` may wear, its own first: one
  // a distinct mesh. Empty for a kind without a mesh or a part out of range.
  [[nodiscard]] const std::vector<int> &choices(PartKind kind, int id) const;
  // It has another mesh to wear.
  [[nodiscard]] bool offers(PartKind kind, int id) const;
  // The look `step` choices on from `look` (-1, or a part of its own mesh,
  // for its own), wrapping round; -1 when that is its own mesh again.
  [[nodiscard]] int step(PartKind kind, int id, int look, int step) const;

private:
  // By mesh kind: body, legs, arm, shoulder; then by part id.
  std::array<std::vector<std::vector<int>>, 4> choices_;
};

// Part `id` of `kind` has a mesh in `catalogue`.
[[nodiscard]] bool has_mesh(const RobotCatalogue &catalogue, PartKind kind, int id);
// The part whose mesh `item` shows: its look, unless `catalogue` is given
// and has no mesh for it (content that is off), then its own.
[[nodiscard]] int worn_part(const StockItem &item, const RobotCatalogue *catalogue = nullptr);

} // namespace phantomcrash
