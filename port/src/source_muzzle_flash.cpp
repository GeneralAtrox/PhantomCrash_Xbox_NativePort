#include "phantomcrash/source_muzzle_flash.hpp"
#include "phantomcrash/skeleton.hpp"
#include <algorithm>
#include <stdexcept>

namespace phantomcrash {
const SourceMuzzleFlashRule *SourceMuzzleFlashes::find(const int weapon_model,
                                                       const bool shoulder) const {
  const auto rule = std::ranges::find_if(rules, [&](const SourceMuzzleFlashRule &r) {
    return r.model == weapon_model && r.shoulder == shoulder;
  });
  return rule == rules.end() ? nullptr : &*rule;
}

SourceMuzzleFlashes parse_source_muzzle_flashes(const json::JsonValue &value) {
  SourceMuzzleFlashes out;
  out.model = value.string_or("model", "");
  if (const auto *layers = value.find("layers")) {
    for (const auto &layer : layers->array()) {
      out.alphas.push_back(static_cast<float>(layer.number_or("alpha", 1.0)));
    }
  }
  if (const auto *colour = value.find("colour"); colour && colour->array().size() == 3) {
    for (std::size_t i = 0; i < 3; ++i) {
      out.colour[i] = static_cast<float>(colour->array()[i].number());
    }
  }
  const double rate = value.number_or("update_hz", 30.0);
  if (rate <= 0) {
    throw std::runtime_error("Source muzzle flash update rate must be positive");
  }
  out.seconds = static_cast<float>(value.number_or("visible_updates", 1.0) / rate);
  if (const auto *roll = value.find("roll"); roll && roll->array().size() == 2) {
    out.roll_min = static_cast<float>(roll->array()[0].number());
    out.roll_max = static_cast<float>(roll->array()[1].number());
  }
  if (const auto *weapons = value.find("weapons")) {
    for (const auto &entry : weapons->array()) {
      SourceMuzzleFlashRule rule;
      rule.model = static_cast<int>(entry.number_or("model", -1));
      rule.shoulder = entry.string_or("mount", "") == "shoulder";
      if (const auto *nodes = entry.find("flash_nodes")) {
        for (const auto &node : nodes->array()) {
          rule.nodes.push_back(static_cast<int>(node.number()));
        }
      }
      rule.barrel_flash = entry.boolean_or("barrel_flash", false);
      rule.scale = static_cast<float>(entry.number_or("flash_scale", 1.0));
      out.rules.push_back(std::move(rule));
    }
  }
  return out;
}

int source_flash_node(const SourceMuzzleFlashRule &rule, const int barrel) {
  if (rule.nodes.empty()) {
    return -1;
  }
  // 0x511630 picks node[+0xb4 != 0]; the table's further nodes are never used.
  const std::size_t index = rule.barrel_flash && barrel > 0 && rule.nodes.size() > 1 ? 1 : 0;
  return rule.nodes[index];
}

Mat4 source_flash_transform(const SourceMuzzleFlashRule &rule, const float roll) {
  return scaling({rule.scale, rule.scale, rule.scale}) * rotation_z(roll);
}
} // namespace phantomcrash
