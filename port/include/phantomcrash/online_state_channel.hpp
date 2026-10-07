#pragma once

#include "online_room.hpp"
#include "online_state.hpp"
#include <functional>
#include <map>

namespace phantomcrash::online {
// Compares immutable, confirmed simulation states. Spectators participate too.
// Reports are retained until compared, independently of rollback history.
class StateChannel {
public:
  using Inspect = std::function<std::optional<StateDigest>(std::uint32_t)>;
  StateChannel(Room &room, std::uint64_t timeline, std::uint32_t first_tick);
  bool receive(const RoomMessage &message);
  void update(std::uint32_t simulated_confirmed_tick, const Inspect &inspect, double now);
  [[nodiscard]] std::uint32_t checked_tick() const {
    return checked_;
  }

private:
  using Reports = std::array<std::optional<StateDigest>, max_players>;
  void compare();
  Room &room_;
  std::uint64_t timeline_{}, next_{};
  std::uint32_t first_{}, checked_{};
  std::uint8_t peers_{};
  std::map<std::uint32_t, Reports> reports_;
};
} // namespace phantomcrash::online
