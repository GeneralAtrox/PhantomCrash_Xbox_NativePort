#include "phantomcrash/online_peer.hpp"
#include "wire.hpp"
#include <limits>

namespace phantomcrash::online {
namespace {
constexpr std::uint32_t magic = 0x4e4f4350; // PCON
constexpr std::uint16_t protocol = 1;
constexpr std::uint8_t data_kind = 1, ack_kind = 2;
constexpr double resend_seconds = 0.05;
constexpr double timeout_seconds = 10.0;

std::vector<std::uint8_t> packet(const std::uint8_t kind, const std::uint64_t token,
                                 const std::uint32_t sequence,
                                 const std::span<const std::uint8_t> payload) {
  WireWriter out;
  out.u32(magic);
  out.u16(protocol);
  out.u8(kind);
  out.u8(0);
  out.u64(token);
  out.u32(sequence);
  out.u16(static_cast<std::uint16_t>(payload.size()));
  out.raw(payload);
  return out.take();
}
} // namespace

ReliablePeer::ReliablePeer(const Endpoint remote, const std::uint64_t token, Send send)
    : remote_(remote), token_(token), send_(std::move(send)) {
  if (remote.port == 0 || token == 0 || !send_) {
    throw std::runtime_error("Invalid online peer");
  }
}

void ReliablePeer::send(std::span<const std::uint8_t> payload, const double now) {
  if (payload.empty() || payload.size() > max_peer_payload || !std::isfinite(now)) {
    throw std::runtime_error("Invalid online peer payload");
  }
  if (pending_.size() >= peer_window) {
    throw std::runtime_error("Online peer is not acknowledging messages");
  }
  if (next_send_ == std::numeric_limits<std::uint32_t>::max()) {
    throw std::runtime_error("Online peer sequence limit reached");
  }
  const auto sequence = next_send_++;
  auto bytes = packet(data_kind, token_, sequence, payload);
  // Own the message even if a nonblocking socket's first send would block.
  auto [position, inserted] = pending_.emplace(sequence, Pending{std::move(bytes), now, now});
  static_cast<void>(inserted);
  static_cast<void>(send_(remote_, position->second.packet));
}

void ReliablePeer::acknowledge(const std::uint32_t sequence) {
  const auto bytes = packet(ack_kind, token_, sequence, {});
  static_cast<void>(send_(remote_, bytes));
}

std::vector<std::vector<std::uint8_t>> ReliablePeer::receive(const Datagram &datagram,
                                                             const double now) {
  std::vector<std::vector<std::uint8_t>> delivered;
  if (datagram.sender != remote_ || datagram.bytes.size() > max_datagram_bytes ||
      !std::isfinite(now)) {
    return delivered;
  }
  std::uint8_t kind{};
  std::uint32_t sequence{};
  std::span<const std::uint8_t> payload;
  try {
    WireReader in(datagram.bytes);
    if (in.u32() != magic || in.u16() != protocol) {
      return delivered;
    }
    kind = in.u8();
    if (in.u8() != 0 || in.u64() != token_) {
      return delivered;
    }
    sequence = in.u32();
    payload = in.raw(in.u16());
    in.finish();
    if (!sequence || (kind != data_kind && kind != ack_kind) ||
        (kind == ack_kind && !payload.empty()) || (kind == data_kind && payload.empty())) {
      return delivered;
    }
  } catch (const std::runtime_error &) {
    return delivered;
  }
  if (kind == ack_kind) {
    pending_.erase(sequence);
    return delivered;
  }
  if (sequence < next_receive_) {
    acknowledge(sequence);
    return delivered;
  }
  if (sequence - next_receive_ >= peer_window) {
    return delivered;
  }
  incoming_.try_emplace(sequence, payload.begin(), payload.end());
  acknowledge(sequence);
  for (;;) {
    auto found = incoming_.find(next_receive_);
    if (found == incoming_.end()) {
      break;
    }
    delivered.push_back(std::move(found->second));
    incoming_.erase(found);
    ++next_receive_;
  }
  return delivered;
}

void ReliablePeer::update(const double now, const bool keep_retrying) {
  if (!std::isfinite(now)) {
    throw std::runtime_error("Invalid online transport clock");
  }
  for (auto &[sequence, pending] : pending_) {
    static_cast<void>(sequence);
    if (!keep_retrying && now - pending.first >= timeout_seconds) {
      throw std::runtime_error("Online peer timed out");
    }
    if (now - pending.last >= resend_seconds) {
      if (send_(remote_, pending.packet)) {
        pending.last = now;
        ++retransmissions_;
      }
    }
  }
}

std::size_t ReliablePeer::pending() const {
  return pending_.size();
}

std::uint64_t ReliablePeer::retransmissions() const {
  return retransmissions_;
}

} // namespace phantomcrash::online
