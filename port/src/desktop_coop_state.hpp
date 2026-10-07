#pragma once

#include "phantomcrash/settings.hpp"
#include <functional>
#include <memory>

namespace phantomcrash {
namespace online {
class Lobby;
}
class CoopMatches;
class OnlineGame;
struct CoopState {
  bool enabled{};
  CoopOptions options;
  int robots{default_rumble_robots};
  std::array<std::optional<QuestSave>, 4> profiles{};
  std::array<std::optional<QuestSave>, 4> saved_profiles{};
  std::shared_ptr<CoopMatches> matches;
  std::shared_ptr<online::Lobby> online;
  std::shared_ptr<OnlineGame> online_game;
  std::function<std::shared_ptr<online::Lobby>()> open_online;
  std::optional<QuestSave> online_profile;
  // Explicit Save completes only after the host has written this profile.
  std::function<bool(int, const QuestSave &)> save_profile;
};
} // namespace phantomcrash
