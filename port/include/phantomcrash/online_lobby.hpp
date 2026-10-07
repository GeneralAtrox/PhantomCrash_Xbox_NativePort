#pragma once

#include "phantomcrash/online_room.hpp"

namespace phantomcrash::online {
struct Presence {
  std::string name;
  std::string screen;
  std::string location;
  bool operator==(const Presence &) const = default;
};

// Only UI location is replicated. Each peer owns its own menus and profile.
class Lobby final {
public:
  explicit Lobby(RoomOptions options);
  void update(double now);
  void set_presence(Presence presence);
  [[nodiscard]] Room &room() {
    return room_;
  }
  [[nodiscard]] const Room &room() const {
    return room_;
  }
  [[nodiscard]] const std::array<Presence, max_players> &presence() const {
    return presence_;
  }
  [[nodiscard]] std::vector<RoomMessage> take_messages();

private:
  bool receive(const RoomMessage &message, double now);
  void publish(std::uint8_t player, double now);
  Room room_;
  Presence local_{"", "lobby", "lobby"};
  bool changed_{true};
  bool presence_pending_{};
  std::uint8_t members_{};
  std::array<Presence, max_players> presence_;
  std::vector<RoomMessage> messages_;
};
} // namespace phantomcrash::online
