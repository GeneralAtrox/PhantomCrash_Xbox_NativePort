#pragma once

#include "phantomcrash/settings.hpp"
#include <functional>
#include <memory>

namespace phantomcrash {
class CoopMatches;
struct CoopState {
  bool enabled{};
  CoopOptions options;
  int robots{default_rumble_robots};
  std::array<std::optional<QuestSave>, 4> profiles{};
  std::array<std::optional<QuestSave>, 4> saved_profiles{};
  std::shared_ptr<CoopMatches> matches;
  // Explicit Save completes only after the host has written this profile.
  std::function<bool(int, const QuestSave &)> save_profile;
};
} // namespace phantomcrash
