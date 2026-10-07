#pragma once

#include "phantomcrash/online_prediction.hpp"
#include "phantomcrash/online_room.hpp"

namespace phantomcrash::online {

// Routes a match's tick inputs through its room host. The host validates the
// assigned sender before relaying; a client can only author its own controls.
// The owner supplies a new nonzero timeline ID for each host-ordered match start.
class InputChannel final {
public:
  InputChannel(Room &room, Prediction &prediction, std::uint64_t timeline, std::uint8_t players);
  void submit(const PlayerInput &input, double now);
  // False: not this timeline's input message; leave it to the room dispatcher.
  // Invalid messages for this timeline throw and must terminate that timeline.
  bool receive(const RoomMessage &message, double now);

private:
  Room &room_;
  Prediction &prediction_;
  std::uint64_t timeline_{};
  std::uint8_t players_{};
  std::optional<PlayerInput> submitted_;
  std::uint8_t peers_{};
  void publish(std::span<const std::uint8_t> bytes, double now);
};

} // namespace phantomcrash::online
