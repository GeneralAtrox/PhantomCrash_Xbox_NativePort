// S.L.A.I.'s story flags in the settings file.

#include "phantomcrash/slai_story_save.hpp"

#include <cstdint>

namespace phantomcrash {

using json::JsonValue;

SlaiStorySave slai_story_from_json(const JsonValue &value) {
  SlaiStorySave save;
  if (!value.is_object()) {
    return save;
  }
  save.tutorial_mark = value.boolean_or("tutorial_mark", false);
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
  for (std::size_t flag = 0; flag < save.flags.size(); ++flag) {
    if (save.flags[flag]) {
      flags.push_back(JsonValue{static_cast<std::int64_t>(flag)});
    }
  }
  return JsonValue{JsonValue::Object{{"flags", JsonValue{std::move(flags)}},
                                     {"tutorial_mark", JsonValue{save.tutorial_mark}}}};
}

} // namespace phantomcrash
