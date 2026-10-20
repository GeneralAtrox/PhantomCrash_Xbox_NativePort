#include "phantomcrash/body_fit.hpp"

#include <cmath>
#include <stdexcept>

namespace phantomcrash {
namespace {

BodyFit parse_fit(const json::JsonValue &value) {
  if (!value.is_object()) {
    throw std::runtime_error("A body fit must be an object");
  }
  BodyFit fit;
  fit.legs_model = static_cast<int>(value.number_or("legs_model", -1));
  fit.body_model = static_cast<int>(value.number_or("body_model", -1));
  fit.scale = static_cast<float>(value.number_or("scale", 1));
  if (!std::isfinite(fit.scale) || fit.scale <= 0) {
    throw std::runtime_error("A body fit's scale must be positive");
  }
  if (const json::JsonValue *offset = value.find("offset")) {
    if (!offset->is_array() || offset->array().size() != 3) {
      throw std::runtime_error("A body fit's offset must be [x, y, z]");
    }
    const auto &xyz = offset->array();
    fit.offset = {static_cast<float>(xyz[0].number()), static_cast<float>(xyz[1].number()),
                  static_cast<float>(xyz[2].number())};
  }
  return fit;
}

template <typename Node> Node *find_mount(Node &node) {
  if ((node.flags & node_turns_with_aim) != 0) {
    return &node;
  }
  for (auto &child : node.children) {
    if (Node *mount = find_mount(child)) {
      return mount;
    }
  }
  return nullptr;
}

} // namespace

BodyFitTable parse_body_fits(const json::JsonValue &document) {
  BodyFitTable table;
  table.enabled = document.boolean_or("enabled", false);
  if (const json::JsonValue *fits = document.find("fits"); fits && fits->is_array()) {
    for (const json::JsonValue &value : fits->array()) {
      table.fits.push_back(parse_fit(value));
    }
  }
  return table;
}

const BodyFit *find_body_fit(const BodyFitTable &table, const int legs_model,
                             const int body_model) {
  if (!table.enabled) {
    return nullptr;
  }
  const BodyFit *best = nullptr;
  int best_rank = 0;
  for (const BodyFit &fit : table.fits) {
    if ((fit.legs_model >= 0 && fit.legs_model != legs_model) ||
        (fit.body_model >= 0 && fit.body_model != body_model)) {
      continue;
    }
    const int rank = (fit.legs_model >= 0 ? 2 : 0) + (fit.body_model >= 0 ? 1 : 0);
    if (best == nullptr || rank > best_rank) {
      best = &fit;
      best_rank = rank;
    }
  }
  return best;
}

Mat4 body_fit_transform(const BodyFit &fit) {
  return scaling({fit.scale, fit.scale, fit.scale}) * translation(fit.offset);
}

bool is_identity_fit(const BodyFit &fit) {
  return fit.scale == 1 && fit.offset.x == 0 && fit.offset.y == 0 && fit.offset.z == 0;
}

std::shared_ptr<const SkeletonNode> fit_body_mount(const SkeletonNode &skeleton, const Mat4 &fit) {
  auto fitted = std::make_shared<SkeletonNode>(skeleton);
  SkeletonNode *mount = find_mount(*fitted);
  if (mount == nullptr) {
    return nullptr;
  }
  mount->body_fit = fit;
  return fitted;
}

} // namespace phantomcrash
