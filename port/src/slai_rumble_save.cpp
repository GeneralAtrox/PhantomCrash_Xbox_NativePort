// S.L.A.I. Rumble's save block in the settings file.

#include "phantomcrash/slai_rumble_save.hpp"

#include <algorithm>
#include <cstdint>
#include <utility>

namespace phantomcrash {
namespace {

using json::JsonValue;

int count_of(const JsonValue &value, const char *key) {
  return std::max(0, static_cast<int>(value.number_or(key, 0)));
}

JsonValue number(const int value) {
  return JsonValue{static_cast<std::int64_t>(value)};
}

} // namespace

SlaiRumbleSave slai_rumble_from_json(const JsonValue &value) {
  SlaiRumbleSave save;
  if (!value.is_object()) {
    return save;
  }
  save.day = count_of(value, "day");
  save.stage = count_of(value, "stage");
  save.city = count_of(value, "city");
  save.finals_cleared = count_of(value, "finals_cleared");
  const auto *records = value.find("records");
  if (records == nullptr || !records->is_array()) {
    return save;
  }
  for (const auto &entry : records->array()) {
    const int id = static_cast<int>(entry.number_or("id", -1));
    if (id < 0) {
      continue;
    }
    SlaiRumbleRecord record;
    record.cleared = entry.boolean_or("cleared", false);
    if (const auto *beaten = entry.find("beaten"); beaten != nullptr && beaten->is_array()) {
      for (const auto &flag : beaten->array()) {
        const int index = flag.is_number() ? static_cast<int>(flag.number()) : 0;
        if (index >= 1 && index <= static_cast<int>(slai_record_flags)) {
          record.beaten[static_cast<std::size_t>(index - 1)] = true;
        }
      }
    }
    if (record != SlaiRumbleRecord{}) {
      save.records[id] = record;
    }
  }
  return save;
}

JsonValue slai_rumble_to_json(const SlaiRumbleSave &save) {
  JsonValue::Array records;
  for (const auto &[id, record] : save.records) {
    if (record == SlaiRumbleRecord{}) {
      continue;
    }
    JsonValue::Array beaten;
    for (std::size_t flag = 0; flag < record.beaten.size(); ++flag) {
      if (record.beaten[flag]) {
        beaten.push_back(number(static_cast<int>(flag) + 1));
      }
    }
    records.push_back(JsonValue{JsonValue::Object{
        {"id", number(id)},
        {"cleared", JsonValue{record.cleared}},
        {"beaten", JsonValue{std::move(beaten)}},
    }});
  }
  return JsonValue{JsonValue::Object{
      {"day", number(save.day)},
      {"stage", number(save.stage)},
      {"city", number(save.city)},
      {"finals_cleared", number(save.finals_cleared)},
      {"records", JsonValue{std::move(records)}},
  }};
}

} // namespace phantomcrash
