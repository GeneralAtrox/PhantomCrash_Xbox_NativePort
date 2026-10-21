// S.L.A.I. Rumble's rota on the calendar (phantomcrash/slai_rota.hpp).

#include "phantomcrash/slai_rota.hpp"

#include "phantomcrash/json_fields.hpp"

#include <stdexcept>
#include <string>

namespace phantomcrash {

using json::required;

SlaiRota parse_slai_rota(const json::JsonValue &document, const int cities, const int slai_days) {
  SlaiRota rota;
  int day = 0;
  for (const auto &entries : required(document, "days").array()) {
    auto &open = rota.days.emplace_back();
    for (const auto &entry : entries.array()) {
      const SlaiRotaEntry value{static_cast<int>(required(entry, "city").number()),
                                static_cast<int>(required(entry, "slai_day").number())};
      if (value.city < 0 || value.city >= cities || value.slai_day < 0 ||
          value.slai_day >= slai_days) {
        throw std::runtime_error("days[" + std::to_string(day) +
                                 "]: a city or slai_day is out of range");
      }
      open.push_back(value);
    }
    ++day;
  }
  return rota;
}

const std::vector<SlaiRotaEntry> &slai_rota_day(const SlaiRota &rota, const int day) {
  static const std::vector<SlaiRotaEntry> none;
  if (rota.days.empty()) {
    return none;
  }
  const int count = static_cast<int>(rota.days.size());
  return rota.days[static_cast<std::size_t>(((day % count) + count) % count)];
}

std::optional<int> slai_rota_source(const SlaiRota &rota, const int day, const int city) {
  for (const SlaiRotaEntry &entry : slai_rota_day(rota, day)) {
    if (entry.city == city) {
      return entry.slai_day;
    }
  }
  return std::nullopt;
}

} // namespace phantomcrash
