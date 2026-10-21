#pragma once

// S.L.A.I. Rumble on Phantom Crash's calendar (docs/SLAI_RUMBLE_PLAN.md,
// "Days"): data/world/slai_rota.json opens three of S.L.A.I.'s seven cities
// each calendar day, each showing its line-up from one S.L.A.I. day near
// it. There is no S.L.A.I. day of its own; the calendar's day picks the
// rota's. The rota is port-authored (slai/rota.py generates it).

#include "phantomcrash/json.hpp"

#include <optional>
#include <vector>

namespace phantomcrash {

inline constexpr const char *slai_rota_file = "world/slai_rota.json";

struct SlaiRotaEntry {
  int city{};     // the table's city column: 0 Tokyo ... 6 Las Vegas
  int slai_day{}; // the S.L.A.I. day (0-based) whose line-up it shows
};

struct SlaiRota {
  // By calendar day, the cities open, in the schedule's display order.
  std::vector<std::vector<SlaiRotaEntry>> days;
};

// Throws std::runtime_error naming the member at fault: a city outside
// [0, cities) or an S.L.A.I. day outside [0, slai_days).
[[nodiscard]] SlaiRota parse_slai_rota(const json::JsonValue &document, int cities,
                                       int slai_days);

// The S.L.A.I. day a city shows on calendar day `day` (wrapping at the
// rota's length), or nothing when the city is closed that day.
[[nodiscard]] std::optional<int> slai_rota_source(const SlaiRota &rota, int day, int city);
// The day's open cities; empty for an empty rota.
[[nodiscard]] const std::vector<SlaiRotaEntry> &slai_rota_day(const SlaiRota &rota, int day);

} // namespace phantomcrash
