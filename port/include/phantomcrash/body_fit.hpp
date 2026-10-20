#pragma once

#include "phantomcrash/json.hpp"
#include "phantomcrash/math3d.hpp"
#include "phantomcrash/skeleton.hpp"

#include <memory>
#include <vector>

namespace phantomcrash {

// Mixed-part body fit: an opt-in port adapter, not recovered behaviour.
// The PAL S.L.A.I. executable joins every body to every legs the same way:
// assembly links the body model to the leg descriptor's first attachment
// node (0x2689a0..9b4 through 0x1b3590, which stores no offset) and sets the
// body's scale to 1.0 (0x2689b8..9d4); part-model creation gives every part
// kind scale 1.0 and translation 0 (0x266694..2667b4); battle construction
// repeats the plain node link (0x4dc8d8..950). No table adjusts a pairing,
// so the shipped table (data/world/body_fit.json) is disabled. A mod may
// scale and move the body, with its arms and shoulder weapons, about the
// mount where it joins the legs.
struct BodyFit {
  int legs_model{-1}; // the leg type's model resource; -1 matches any legs
  int body_model{-1}; // the body type's model resource; -1 matches any body
  float scale{1};     // uniform, about the mount
  Vec3 offset{};      // in the mount's space, after scaling
};

struct BodyFitTable {
  bool enabled{};
  std::vector<BodyFit> fits;
};

// {"enabled": false, "fits": [{"legs_model": 14102, "body_model": 12002,
//  "scale": 1.0, "offset": [0, 0, 0]}]}; throws std::runtime_error on a
// non-positive scale or a malformed offset.
[[nodiscard]] BodyFitTable parse_body_fits(const json::JsonValue &document);

// The entry for the pairing when the table is enabled: one naming both
// models wins over one naming the legs, which wins over one naming the
// body; the first of equally specific entries. Null when none applies.
[[nodiscard]] const BodyFit *find_body_fit(const BodyFitTable &table, int legs_model,
                                           int body_model);

// The fit as a local transform placed before the mount's own (row vectors).
[[nodiscard]] Mat4 body_fit_transform(const BodyFit &fit);
[[nodiscard]] bool is_identity_fit(const BodyFit &fit);

// A copy of `skeleton` whose mount (the first node that turns with the aim)
// carries `fit`, or null when it has no such node.
[[nodiscard]] std::shared_ptr<const SkeletonNode> fit_body_mount(const SkeletonNode &skeleton,
                                                                 const Mat4 &fit);

} // namespace phantomcrash
