#include "phantomcrash/online_room.hpp"
#include "wire.hpp"
#include <algorithm>
#include <random>
#include <utility>

namespace phantomcrash::online {
namespace {
constexpr std::uint32_t handshake_magic = 0x484f4350; // PCOH
constexpr std::uint16_t protocol = 1;
constexpr std::uint8_t hello = 1, welcome = 2, incompatible = 3, full = 4;
constexpr std::uint8_t heartbeat = 0, application = 1;
constexpr double hello_interval = 0.25, heartbeat_interval = 0.5, timeout = 10;
constexpr std::size_t receive_budget = 64, message_limit = 512;

std::uint64_t nonce() {
  std::random_device source;
  const auto result = (static_cast<std::uint64_t>(source()) << 32) | source();
  return result ? result : 1;
}
} // namespace

Room::Room(RoomOptions options)
    : options_(std::move(options)), socket_(options_.bind), nonce_(nonce()) {
  if (options_.capacity < 2 || options_.capacity > max_players ||
      (!options_.host && options_.server.port == 0)) {
    throw std::runtime_error("Invalid online room configuration");
  }
  if (options_.host) {
    token_ = nonce();
    local_player_ = 0;
  }
}

Room::~Room() = default;

std::unique_ptr<ReliablePeer> Room::channel(const Endpoint remote) {
  return std::make_unique<ReliablePeer>(
      remote, token_, [this](Endpoint target, std::span<const std::uint8_t> bytes) {
        return socket_.send(target, bytes);
      });
}

void Room::send_handshake(const Endpoint target, const std::uint8_t kind, const std::uint8_t slot,
                          const std::uint64_t request_nonce) {
  WireWriter out;
  out.u32(handshake_magic);
  out.u16(protocol);
  out.u8(kind);
  out.u8(slot);
  out.u64(request_nonce);
  out.u64(token_);
  out.u32(options_.seed);
  out.u8(options_.capacity);
  out.raw(options_.compatibility);
  const auto bytes = out.take();
  static_cast<void>(socket_.send(target, bytes));
}

bool Room::handshake(const Datagram &packet, const double now) {
  WireReader in(packet.bytes);
  if (packet.bytes.size() < 4 || in.u32() != handshake_magic) {
    return false;
  }
  try {
    const auto version = in.u16();
    const auto kind = in.u8();
    const auto slot = in.u8();
    const auto request_nonce = in.u64();
    const auto token = in.u64();
    const auto seed = in.u32();
    const auto capacity = in.u8();
    const auto compatibility = in.raw(options_.compatibility.size());
    in.finish();
    if (!request_nonce) {
      return true;
    }
    if (options_.host) {
      if (kind != hello) {
        return true;
      }
      if (version != protocol || !std::ranges::equal(compatibility, options_.compatibility)) {
        send_handshake(packet.sender, incompatible, 0, request_nonce);
        return true;
      }
      for (std::uint8_t player = 1; player < options_.capacity; ++player) {
        auto &peer = peers_[player];
        if (peer.channel && peer.nonce == request_nonce) {
          if (peer.endpoint != packet.sender) {
            peer.endpoint = packet.sender;
            peer.channel->rebind(packet.sender);
          }
          send_handshake(packet.sender, welcome, player, request_nonce);
          return true;
        }
      }
      for (std::uint8_t player = 1; player < options_.capacity; ++player) {
        auto &peer = peers_[player];
        if (!peer.channel) {
          peer = {packet.sender, request_nonce, channel(packet.sender), now, now, false};
          send_handshake(packet.sender, welcome, player, request_nonce);
          return true;
        }
      }
      send_handshake(packet.sender, full, 0, request_nonce);
      return true;
    }
    if (packet.sender != options_.server || request_nonce != nonce_) {
      return true;
    }
    if (kind == incompatible) {
      error_ = "The host has a different game build or gameplay content";
      return true;
    }
    if (kind == full) {
      error_ = "The online room is full";
      return true;
    }
    if (kind != welcome || version != protocol || !token || slot == 0 || slot >= capacity ||
        capacity > max_players || !std::ranges::equal(compatibility, options_.compatibility)) {
      return true;
    }
    if (!peers_[0].channel) {
      token_ = token;
      local_player_ = slot;
      options_.capacity = capacity;
      options_.seed = seed;
      peers_[0] = {packet.sender, request_nonce, channel(packet.sender), now, now, true};
      membership_ = static_cast<std::uint8_t>(1U | (1U << slot));
      peers_[0].channel->send(std::array<std::uint8_t, 2>{heartbeat, membership_}, now);
    }
  } catch (const std::runtime_error &) {
    // A truncated/malformed handshake cannot change the established session.
  }
  return true;
}

void Room::receive(Peer &peer, const std::uint8_t slot, const Datagram &packet, const double now) {
  for (auto &payload : peer.channel->receive(packet, now)) {
    if (payload.empty() || (payload[0] != heartbeat && payload[0] != application)) {
      continue;
    }
    peer.heard = now;
    const bool newly_ready = !peer.ready;
    peer.ready = true;
    if (payload[0] == heartbeat && payload.size() == 2 && !options_.host &&
        payload[1] < (1U << max_players) && (payload[1] & 1U) &&
        (payload[1] & (1U << local_player_))) {
      membership_ = payload[1];
    }
    if (newly_ready && options_.host) {
      for (auto &participant : peers_) {
        if (participant.ready && participant.channel->pending() < peer_window) {
          participant.channel->send(std::array<std::uint8_t, 2>{heartbeat, participants()}, now);
        }
      }
    }
    if (payload[0] == application) {
      if (messages_.size() == message_limit) {
        throw std::runtime_error("Online application is not consuming messages");
      }
      messages_.push_back({slot, {payload.begin() + 1, payload.end()}});
    }
  }
}

void Room::update(const double now) {
  if (!error_.empty()) {
    return;
  }
  if (!std::isfinite(now)) {
    throw std::runtime_error("Invalid online room clock");
  }
  if (!started_) {
    started_ = now;
  }
  now_ = now;
  try {
    if (!options_.host && (!connected() || recovering())) {
      if (!connected() && now - *started_ >= timeout) {
        error_ = "Could not reach the online host";
        return;
      }
      if (!hello_ || now - *hello_ >= hello_interval) {
        send_handshake(options_.server, hello, max_players, nonce_);
        hello_ = now;
      }
    }
    for (std::size_t count = 0; count < receive_budget; ++count) {
      auto packet = socket_.receive();
      if (!packet) {
        break;
      }
      if (handshake(*packet, now)) {
        continue;
      }
      for (std::uint8_t slot = 0; slot < max_players; ++slot) {
        auto &peer = peers_[slot];
        if (peer.channel && peer.endpoint == packet->sender) {
          receive(peer, slot, *packet, now);
          break;
        }
      }
    }
    for (auto &peer : peers_) {
      if (!peer.channel) {
        continue;
      }
      if (peer.ready && now - peer.heartbeat >= heartbeat_interval &&
          peer.channel->pending() < peer_window) {
        peer.channel->send(std::array<std::uint8_t, 2>{heartbeat, participants()}, now);
        peer.heartbeat = now;
      }
      peer.channel->update(now, true);
    }
  } catch (const std::runtime_error &error) {
    error_ = error.what();
  }
}

void Room::send(const std::uint8_t recipient, std::span<const std::uint8_t> bytes,
                const double now) {
  if (recipient >= max_players || !peers_[recipient].ready || !error_.empty() || bytes.empty() ||
      bytes.size() >= max_peer_payload) {
    throw std::runtime_error("Online recipient is unavailable or message is invalid");
  }
  WireWriter out;
  out.u8(application);
  out.raw(bytes);
  const auto payload = out.take();
  peers_[recipient].channel->send(payload, now);
}

void Room::broadcast(std::span<const std::uint8_t> bytes, const double now) {
  for (std::uint8_t slot = 0; slot < max_players; ++slot) {
    if (peers_[slot].ready) {
      send(slot, bytes, now);
    }
  }
}

std::size_t Room::send_capacity(const std::uint8_t recipient) const {
  if (recipient >= max_players || !peers_[recipient].ready || !error_.empty()) {
    return 0;
  }
  return peer_window - peers_[recipient].channel->pending();
}

std::vector<RoomMessage> Room::take_messages() {
  return std::exchange(messages_, {});
}

std::uint8_t Room::local_player() const {
  return local_player_;
}

std::uint8_t Room::participants() const {
  if (!options_.host) {
    return connected() ? membership_ : 0;
  }
  std::uint8_t mask = connected() ? static_cast<std::uint8_t>(1U << local_player_) : 0;
  for (std::uint8_t slot = 0; slot < max_players; ++slot) {
    if (peers_[slot].ready) {
      mask |= static_cast<std::uint8_t>(1U << slot);
    }
  }
  return mask;
}

std::uint32_t Room::seed() const {
  return options_.seed;
}

bool Room::connected() const {
  return local_player_ < max_players && error_.empty();
}

bool Room::recovering() const {
  return std::ranges::any_of(peers_, [this](const Peer &peer) {
    return peer.channel &&
           (now_ - peer.heard >= timeout || peer.channel->pending() > peer_window - 16);
  });
}

Endpoint Room::local_endpoint() const {
  return socket_.local();
}

const std::string &Room::error() const {
  return error_;
}

} // namespace phantomcrash::online
