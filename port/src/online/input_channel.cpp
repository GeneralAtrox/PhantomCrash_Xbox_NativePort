#include "phantomcrash/online_input_channel.hpp"
#include "wire.hpp"

namespace phantomcrash::online {
namespace {
constexpr std::uint32_t input_message = 0x494f4350; // PCOI
constexpr std::uint16_t version = 1;

std::vector<std::uint8_t> packet(const std::uint64_t timeline, const PlayerInput &input) {
  WireWriter out;
  out.u32(input_message);
  out.u16(version);
  out.u64(timeline);
  const auto bytes = encode_input(input);
  out.u16(static_cast<std::uint16_t>(bytes.size()));
  out.raw(bytes);
  return out.take();
}
} // namespace

InputChannel::InputChannel(Room &room, Prediction &prediction, const std::uint64_t timeline,
                           const std::uint8_t players)
    : room_(room), prediction_(prediction), timeline_(timeline), players_(players),
      peers_(room.local_player() == 0 ? room.participants() : 1) {
  if (!room.connected() || !timeline || !players || players >= (1U << max_players) ||
      (room.participants() & players) != players) {
    throw std::runtime_error("Invalid online input channel");
  }
}

void InputChannel::submit(const PlayerInput &input, const double now) {
  if (!room_.connected() || input.player != room_.local_player() ||
      !(players_ & (1U << input.player)) || input.tick != prediction_.next_tick()) {
    throw std::runtime_error("Cannot submit another player's or tick's controls");
  }
  if (submitted_ && submitted_->tick == input.tick) {
    if (!same_input(*submitted_, input)) {
      throw std::runtime_error("Cannot replace controls already sent for a stalled tick");
    }
    return; // Transport already owns retransmission while prediction is stalled.
  }
  const auto bytes = packet(timeline_, input);
  prediction_.submit(input);
  publish(bytes, now);
  submitted_ = input;
}

bool InputChannel::receive(const RoomMessage &message, const double now) {
  if (message.bytes.size() < 4) {
    return false;
  }
  WireReader in(message.bytes);
  if (in.u32() != input_message) {
    return false;
  }
  if (in.u16() != version) {
    throw std::runtime_error("Unsupported online input protocol");
  }
  if (in.u64() != timeline_) {
    return false;
  }
  const auto bytes = in.raw(in.u16());
  in.finish();
  const auto input = decode_input(bytes);
  const bool host = room_.local_player() == 0;
  if (!(players_ & (1U << input.player)) ||
      (host ? message.sender != input.player || message.sender == 0 : message.sender != 0)) {
    throw std::runtime_error("Online input does not belong to its assigned sender");
  }
  prediction_.submit(input);
  if (host) {
    publish(message.bytes, now);
  }
  return true;
}

void InputChannel::publish(const std::span<const std::uint8_t> bytes, const double now) {
  for (std::uint8_t peer = 0; peer < max_players; ++peer) {
    if (peer != room_.local_player() && (peers_ & (1U << peer))) {
      room_.send(peer, bytes, now);
    }
  }
}

} // namespace phantomcrash::online
