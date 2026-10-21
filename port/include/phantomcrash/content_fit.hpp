#pragma once

#include "phantomcrash/garage.hpp"
#include "phantomcrash/settings.hpp"

namespace phantomcrash {

// How many records of each kind the loaded tables hold (tables/parts.json
// and tables/garage.json). S.L.A.I. content appends to every list, so a part
// id at or past a count is S.L.A.I.'s while S.L.A.I. is off.
struct ContentLimits {
  int bodies{};
  int legs{};
  int arm_weapons{};
  int shoulder_weapons{};
  int chips{};
  int modules{};
  int paints{};
  int emblems{};

  // Every list is there; a fit against anything less would set the whole
  // garage aside.
  [[nodiscard]] bool valid() const;
  [[nodiscard]] bool holds(const StockItem &item) const;
};

[[nodiscard]] ContentLimits content_limits(const json::JsonValue &parts,
                                           const json::JsonValue &garage);

// Gives back what was set aside and the limits hold again, then sets aside
// the stock entries, hangars, paints and emblems they do not hold. Nothing
// is lost: a later fit with the content back restores it all.
void fit_garage(Garage &garage, const ContentLimits &limits);

// Fits every game the settings keep: the current one, the save slots and
// the co-op and online profiles.
void fit_settings(Settings &settings, const ContentLimits &limits);

} // namespace phantomcrash
