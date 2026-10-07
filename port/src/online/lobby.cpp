#include "phantomcrash/online_lobby.hpp"
#include "wire.hpp"
#include <utility>

namespace phantomcrash::online {
namespace {
constexpr std::uint32_t magic = 0x504f4350; // PCOP
constexpr std::size_t text_limit = 128;
void write_text(WireWriter &out, const std::string &value) {
  if (value.size() > text_limit) {
    throw std::runtime_error("Online presence text is too long");
  }
  out.u16(static_cast<std::uint16_t>(value.size()));
  out.raw({reinterpret_cast<const std::uint8_t *>(value.data()), value.size()});
}
std::string read_text(WireReader &in) {
  const auto size = in.u16();
  if (size > text_limit) {
    throw std::runtime_error("Online presence text is too long");
  }
  const auto bytes = in.raw(size);
  return {bytes.begin(), bytes.end()};
}
} // namespace

Lobby::Lobby(RoomOptions options) : room_(std::move(options)) {}

void Lobby::set_presence(Presence presence) {
  if (presence.name.size() > text_limit || presence.screen.size() > text_limit ||
      presence.location.size() > text_limit) {
    throw std::runtime_error("Online presence text is too long");
  }
  if (local_ != presence) {
    local_ = std::move(presence);
    changed_ = true;
  }
}

void Lobby::publish(const std::uint8_t player, const double now) {
  if (room_.recovering()) {
    presence_pending_ = true;
    return;
  }
  WireWriter out;
  out.u32(magic);
  out.u8(player);
  write_text(out, presence_[player].name);
  write_text(out, presence_[player].screen);
  write_text(out, presence_[player].location);
  const auto bytes = out.take();
  room_.broadcast(bytes, now);
}

bool Lobby::receive(const RoomMessage &message, const double now) {
  if (message.bytes.size() < 4) {
    return false;
  }
  WireReader in(message.bytes);
  if (in.u32() != magic) {
    return false;
  }
  const auto player = in.u8();
  Presence value{read_text(in), read_text(in), read_text(in)};
  in.finish();
  if (player >= max_players || !(room_.participants() & (1U << player)) ||
      (room_.local_player() == 0 ? message.sender != player || player == 0 : message.sender != 0)) {
    throw std::runtime_error("Online presence does not belong to its assigned sender");
  }
  presence_[player] = std::move(value);
  if (room_.local_player() == 0) {
    publish(player, now);
  }
  return true;
}

void Lobby::update(const double now) {
  room_.update(now);
  if (!room_.connected()) {
    return;
  }
  for (auto &message : room_.take_messages()) {
    if (!receive(message, now)) {
      messages_.push_back(std::move(message));
    }
  }
  const auto members = room_.participants();
  if (room_.recovering()) {
    return;
  }
  if (changed_) {
    presence_[room_.local_player()] = local_;
    publish(room_.local_player(), now);
    changed_ = false;
  }
  if ((members_ != members || presence_pending_) && room_.local_player() == 0) {
    presence_pending_ = false;
    for (std::uint8_t slot = 0; slot < max_players; ++slot) {
      if (members & (1U << slot)) {
        publish(slot, now);
      }
    }
  } else if (presence_pending_) {
    presence_pending_ = false;
    publish(room_.local_player(), now);
  }
  members_ = members;
}

std::vector<RoomMessage> Lobby::take_messages() {
  return std::exchange(messages_, {});
}
} // namespace phantomcrash::online
