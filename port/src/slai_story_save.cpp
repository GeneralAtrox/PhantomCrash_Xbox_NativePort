// S.L.A.I.'s story flags and counts in the settings file.

#include "phantomcrash/slai_story_save.hpp"

#include <algorithm>
#include <cstdint>
#include <string_view>

namespace phantomcrash {

using json::JsonValue;

namespace {

constexpr int max_days = 9999; // 0x4E8CF0's cap

int count_or(const JsonValue &value, std::string_view key, int fallback, int low, int high) {
  return std::clamp(static_cast<int>(value.number_or(key, fallback)), low, high);
}

} // namespace

SlaiStorySave slai_story_from_json(const JsonValue &value) {
  SlaiStorySave save;
  if (!value.is_object()) {
    return save;
  }
  save.days = count_or(value, "days", 0, 0, max_days);
  save.nights_seen = count_or(value, "nights_seen", -1, -1, max_days * 100);
  save.defeated =
      std::max<std::int64_t>(0, static_cast<std::int64_t>(value.number_or("defeated", 0)));
  save.sponsor = count_or(value, "sponsor", slai_no_sponsor, 0, slai_no_sponsor);
  save.previous_sponsor = count_or(value, "previous_sponsor", slai_no_sponsor, 0, slai_no_sponsor);
  save.sponsors_had = count_or(value, "sponsors_had", 0, 0, (1 << slai_no_sponsor) - 1);
  save.shot_down_streak = count_or(value, "shot_down_streak", 0, 0, max_days);
  save.dismissal = count_or(value, "dismissal", 0, 0, 3);
  save.survived = value.boolean_or("survived", false);
  const auto *flags = value.find("flags");
  if (flags == nullptr || !flags->is_array()) {
    return save;
  }
  for (const auto &entry : flags->array()) {
    if (!entry.is_number()) {
      continue;
    }
    const auto flag = static_cast<std::int64_t>(entry.number());
    if (flag >= 0 && flag < static_cast<std::int64_t>(slai_story_flags)) {
      save.flags[static_cast<std::size_t>(flag)] = true;
    }
  }
  return save;
}

std::vector<int> slai_story_flags_set(const SlaiStorySave &save) {
  std::vector<int> set;
  for (std::size_t flag = 0; flag < save.flags.size(); ++flag) {
    if (save.flags[flag]) {
      set.push_back(static_cast<int>(flag));
    }
  }
  return set;
}

JsonValue slai_story_to_json(const SlaiStorySave &save) {
  JsonValue::Array flags;
  for (const int flag : slai_story_flags_set(save)) {
    flags.push_back(JsonValue{static_cast<std::int64_t>(flag)});
  }
  const auto number = [](const std::int64_t value) { return JsonValue{value}; };
  return JsonValue{JsonValue::Object{{"flags", JsonValue{std::move(flags)}},
                                     {"days", number(save.days)},
                                     {"nights_seen", number(save.nights_seen)},
                                     {"defeated", number(save.defeated)},
                                     {"sponsor", number(save.sponsor)},
                                     {"previous_sponsor", number(save.previous_sponsor)},
                                     {"sponsors_had", number(save.sponsors_had)},
                                     {"shot_down_streak", number(save.shot_down_streak)},
                                     {"dismissal", number(save.dismissal)},
                                     {"survived", JsonValue{save.survived}}}};
}

} // namespace phantomcrash
