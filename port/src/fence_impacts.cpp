#include "phantomcrash/fence_impacts.hpp"
#include "phantomcrash/json_fields.hpp"

#include <algorithm>
#include <cmath>
#include <stdexcept>

namespace phantomcrash {
namespace {
Mat4 surface_frame(const Vec3 at, const Vec3 normal) {
  const Vec3 y = normalize(normal);
  const Vec3 helper = std::abs(y.y) < 0.99F ? Vec3{0, 1, 0} : Vec3{1, 0, 0};
  const Vec3 x = normalize(cross(helper, y));
  const Vec3 z = cross(x, y);
  Mat4 out = translation(at);
  const Vec3 axes[] = {x, y, z};
  for (int row = 0; row < 3; ++row) {
    out.at(row, 0) = axes[row].x;
    out.at(row, 1) = axes[row].y;
    out.at(row, 2) = axes[row].z;
  }
  return out;
}
} // namespace

FenceLook parse_fence_look(const json::JsonValue &value) {
  FenceLook out;
  out.model = value.string_or("model", "");
  if (const auto *meshes = value.find("meshes")) {
    for (const auto &mesh : meshes->array()) {
      out.meshes.push_back(static_cast<int>(mesh.number()));
    }
  }
  out.capacity = static_cast<int>(value.number_or("capacity", out.capacity));
  out.frame_rate = static_cast<float>(value.number_or("frame_rate", out.frame_rate));
  out.scale = static_cast<float>(value.number_or("scale", out.scale));
  out.growth = static_cast<float>(value.number_or("growth", out.growth));
  out.depth = static_cast<float>(value.number_or("depth", out.depth));
  out.offset = static_cast<float>(value.number_or("offset", out.offset));
  if (const auto *refraction = value.find("refraction")) {
    out.refraction.edge = static_cast<float>(refraction->number_or("edge", out.refraction.edge));
    out.refraction.reach = static_cast<float>(refraction->number_or("reach", out.refraction.reach));
    out.refraction.push = static_cast<float>(refraction->number_or("push", out.refraction.push));
    out.refraction.least = static_cast<float>(refraction->number_or("least", out.refraction.least));
  }
  if (out.capacity < 1 || out.frame_rate <= 0 || out.refraction.edge <= 0 ||
      out.refraction.reach <= 0) {
    throw std::runtime_error("Invalid fence impact timing or refraction");
  }
  return out;
}

void FenceImpacts::start(const StageHit &hit) {
  if ((surface_flags(hit.surface) & stage_contact::fence) == 0 || look_.meshes.empty() ||
      impacts_.size() >= static_cast<std::size_t>(look_.capacity)) {
    return;
  }
  impacts_.push_back({hit.position + hit.normal * look_.offset, hit.normal});
}

void FenceImpacts::tick(const float seconds) {
  for (auto &impact : impacts_) {
    impact.age += static_cast<double>(seconds) * look_.frame_rate;
  }
  std::erase_if(impacts_, [&](const Impact &impact) {
    return impact.age >= static_cast<double>(look_.meshes.size());
  });
}

std::vector<FenceFrame> FenceImpacts::frames() const {
  std::vector<FenceFrame> out;
  for (const auto &impact : impacts_) {
    const auto frame = static_cast<std::size_t>(impact.age);
    const float scale = look_.scale + look_.growth * static_cast<float>(frame) /
                                          static_cast<float>(look_.meshes.size());
    OpticalSphere refraction = look_.refraction;
    refraction.centre = impact.at;
    out.push_back({look_.meshes[frame],
                   scaling({scale, look_.depth, scale}) * surface_frame(impact.at, impact.normal),
                   refraction});
  }
  return out;
}

} // namespace phantomcrash
