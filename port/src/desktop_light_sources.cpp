#include "desktop_light_sources.hpp"
#include "phantomcrash/json_fields.hpp"
#include <cmath>
#include <stdexcept>

namespace phantomcrash {
std::vector<WorldLight> light_sources(const DataRoots &roots, const char *group, int stage) {
  std::vector<WorldLight> out;
  const auto data = roots.document("world/dynamic_lighting.json");
  const auto *groups = data.find(group);
  const auto *entries = groups ? groups->find(std::to_string(stage)) : nullptr;
  if (!entries) {
    return out;
  }
  for (const auto &entry : entries->array()) {
    const auto vector = [&](const char *key) {
      const auto a = json::numbers<3>(json::required(entry, key), key);
      for (float x : a) {
        if (!std::isfinite(x)) {
          throw std::runtime_error("Non-finite light vector");
        }
      }
      return Vec3{a[0], a[1], a[2]};
    };
    WorldLight light;
    light.position = vector("position");
    light.direction = normalize(vector("direction"));
    light.colour = vector("colour");
    light.intensity = static_cast<float>(entry.number_or("intensity", 2));
    light.range = static_cast<float>(entry.number_or("range", 200));
    light.inner_degrees = static_cast<float>(entry.number_or("inner_degrees", 20));
    light.outer_degrees = static_cast<float>(entry.number_or("outer_degrees", 30));
    if (!std::isfinite(light.intensity) || light.intensity < 0 || !std::isfinite(light.range) ||
        light.range <= 0.5F ||
        !(light.inner_degrees >= 0 && light.inner_degrees < light.outer_degrees &&
          light.outer_degrees < 89) ||
        length(light.direction) < 0.9F || light.colour.x < 0 || light.colour.y < 0 ||
        light.colour.z < 0) {
      throw std::runtime_error("Invalid dynamic spotlight settings");
    }
    out.push_back(light);
  }
  return out;
}
} // namespace phantomcrash
