// The stats a shop's panel has no printed line for (part_stats.hpp).

#include "phantomcrash/part_stats.hpp"

#include "phantomcrash/json_fields.hpp"

#include <algorithm>
#include <cmath>
#include <stdexcept>

namespace phantomcrash {
namespace {

template <typename T> const T *record_at(const std::vector<T> &records, const int index) {
  return index >= 0 && index < static_cast<int>(records.size())
             ? &records[static_cast<std::size_t>(index)]
             : nullptr;
}

// The goods' value of `source`; nullopt where the kind has no such stat.
std::optional<double> extra_value(const PartTables &parts, const GarageTables &garage,
                                  const Goods &goods, const ExtraStatSource source) {
  switch (goods.kind) {
  case GoodsKind::module:
    if (const ModuleRecord *module = record_at(garage.modules, goods.id)) {
      if (source == ExtraStatSource::critical_percent) {
        return module->critical_percent;
      }
      if (source == ExtraStatSource::knockdown_resistance) {
        return module->knockdown_resistance;
      }
    }
    return std::nullopt;
  case GoodsKind::chip:
    if (const ChipRecord *chip = parts.chip(goods.id)) {
      switch (source) {
      case ExtraStatSource::critical_percent:
        return chip->critical_percent.value(goods.level);
      case ExtraStatSource::dodge_percent:
        return chip->dodge_percent.value(goods.level);
      case ExtraStatSource::knockdown_resistance:
        return chip->knockdown_resistance.value(goods.level);
      default:
        return std::nullopt;
      }
    }
    return std::nullopt;
  case GoodsKind::legs:
    if (const ArmourRecord *legs = record_at(parts.legs, goods.id);
        legs != nullptr && source == ExtraStatSource::knockback_resistance) {
      return legs->knockback_resistance.value(goods.level);
    }
    return std::nullopt;
  default:
    return std::nullopt;
  }
}

std::optional<ExtraStatSource> source_named(const std::string &name) {
  if (name == "critical_percent") {
    return ExtraStatSource::critical_percent;
  }
  if (name == "dodge_percent") {
    return ExtraStatSource::dodge_percent;
  }
  if (name == "knockdown_resistance") {
    return ExtraStatSource::knockdown_resistance;
  }
  if (name == "knockback_resistance") {
    return ExtraStatSource::knockback_resistance;
  }
  return std::nullopt;
}

} // namespace

std::array<std::vector<ExtraStat>, goods_kind_count>
parse_extra_stats(const json::JsonValue &document) {
  std::array<std::vector<ExtraStat>, goods_kind_count> extras;
  const json::JsonValue *table = document.find("extras");
  if (table == nullptr) {
    return extras;
  }
  for (const auto &[name, list] : table->object()) {
    const auto kind = goods_kind_named(name);
    if (!kind) {
      throw std::runtime_error("extras: no kind " + name);
    }
    for (const json::JsonValue &entry : list.array()) {
      ExtraStat stat;
      const std::string source = json::required(entry, "stat").string();
      const auto parsed = source_named(source);
      if (!parsed) {
        throw std::runtime_error("extras." + name + ": no stat " + source);
      }
      stat.source = *parsed;
      stat.label = json::required(entry, "label").string();
      stat.unit = entry.string_or("unit", "");
      stat.decimals = std::clamp(static_cast<int>(entry.number_or("decimals", 0)), 0, 6);
      extras[static_cast<std::size_t>(*kind)].push_back(std::move(stat));
    }
  }
  return extras;
}

std::vector<ExtraStatLine> extra_stats(const PartStatsTable &table, const PartTables &parts,
                                       const GarageTables &garage, const Goods &goods) {
  std::vector<ExtraStatLine> lines;
  for (const ExtraStat &stat : table.extras[static_cast<std::size_t>(goods.kind)]) {
    const auto value = extra_value(parts, garage, goods, stat.source);
    if (!value) {
      continue;
    }
    // Only what the goods change: a 0 shows nothing.
    const double scale = std::pow(10.0, stat.decimals);
    if (std::abs(std::trunc(*value * scale)) < 1.0) {
      continue;
    }
    lines.push_back({stat.label, stat.unit, stat_text({StatRow::level, *value, stat.decimals})});
  }
  return lines;
}

} // namespace phantomcrash
