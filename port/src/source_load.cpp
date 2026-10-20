#include "phantomcrash/source_load.hpp"

#include <cmath>
#include <stdexcept>

namespace phantomcrash {

std::optional<SourceLoadRule> parse_source_load(const json::JsonValue *value) {
  if (value == nullptr) {
    return std::nullopt;
  }
  SourceLoadRule out;
  out.level_max = static_cast<int>(value->number_or("level_max", out.level_max));
  out.level_trim = static_cast<float>(value->number_or("level_trim", out.level_trim));
  out.overload_band = static_cast<float>(value->number_or("overload_band", out.overload_band));
  out.graded_bands = static_cast<int>(value->number_or("graded_bands", out.graded_bands));
  out.overload_power = static_cast<float>(value->number_or("overload_power", out.overload_power));
  if (out.level_max <= 0 || !(out.overload_band > 0.0F) || out.graded_bands < 0) {
    throw std::runtime_error("Invalid S.L.A.I. legs load rule");
  }
  return out;
}

float source_load_factor(const SourceLoadRule &rule, const float frame, const float weight,
                         const float capacity, const int legs_level) {
  if (weight == 0.0F) {
    return 1.0F; // 0x2568ec
  }
  const float trim =
      1.0F - static_cast<float>(legs_level) / static_cast<float>(rule.level_max) * rule.level_trim;
  float out = std::sqrt(frame / weight) * trim;
  if (weight > capacity) {
    // 0x256990..2569b8: the band, truncated, counting from 1.
    const int band = static_cast<int>((weight - capacity) / rule.overload_band) + 1;
    const float power =
        band <= rule.graded_bands ? static_cast<float>(band * band) : rule.overload_power;
    out *= std::pow(capacity / weight, power); // 0x256a08
  }
  return out;
}

} // namespace phantomcrash
