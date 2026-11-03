#include "phantomcrash/online_relay.hpp"
#include "relay_protocol.hpp"
#include <algorithm>
#include <stdexcept>

// Finding a direct path: both members send to each other's addresses, which
// opens each one's router to the other. Only an answered probe proves a path.
namespace phantomcrash::online {
using namespace relay_protocol;

namespace {
// Directly after learning a peer's addresses routers are most likely to open;
// later tries stay cheap in case a peer's router only opens once it sends too.
constexpr double probe_fast = 0.25, probe_fast_for = 10.0, probe_slow = 2.0;
constexpr double direct_probe = 0.5, direct_lost_after = 2.0;
} // namespace

void RelayClient::send_probes(const double now) {
  const bool blocked = direct_blocked();
  for (std::uint8_t member = 0; member < routes_.size(); ++member) {
    auto &route = routes_[member];
    if (!route.present || route.candidates.empty()) {
      continue;
    }
    if (route.direct && now - route.heard >= direct_lost_after) {
      route.direct.reset();
      status_.direct[member] = false;
      ++status_.fell_back;
      route.next_probe = now + probe_slow;
    }
    if (blocked || now < route.next_probe) {
      continue;
    }
    WireWriter out;
    open(out, probe);
    out.u8(status_.member);
    out.u64(room_key_);
    route.probe_nonce = random_u64();
    out.u64(route.probe_nonce);
    const auto bytes = out.take();
    route.probe_sent = now;
    if (route.direct) {
      static_cast<void>(socket_.send(*route.direct, bytes));
      route.next_probe = now + direct_probe;
      continue;
    }
    for (const auto candidate : route.candidates) {
      try {
        static_cast<void>(socket_.send(candidate, bytes));
      } catch (const std::runtime_error &) {
        // An address another network cannot route to is one failed candidate.
      }
    }
    route.next_probe = now + (now - route.first_seen < probe_fast_for ? probe_fast : probe_slow);
  }
}

bool RelayClient::handle_probe(const Datagram &packet) {
  if (packet.bytes.size() < 5) {
    return false;
  }
  try {
    WireReader in(packet.bytes);
    if (in.u32() != magic) {
      return false;
    }
    const auto kind = in.u8();
    const auto member = in.u8();
    const auto key = in.u64();
    const auto nonce = in.u64();
    in.finish();
    if ((kind != probe && kind != probe_ack) || key != room_key_ || !room_key_ ||
        member >= routes_.size() || member == status_.member || options_.relay_only ||
        direct_blocked()) {
      return true;
    }
    known_[packet.sender] = member;
    if (kind == probe) {
      WireWriter out;
      open(out, probe_ack);
      out.u8(status_.member);
      out.u64(room_key_);
      out.u64(nonce);
      static_cast<void>(socket_.send(packet.sender, out.take()));
      return true;
    }
    auto &route = routes_[member];
    if (nonce != route.probe_nonce) {
      return true;
    }
    route.heard = now_;
    status_.direct_round_trip[member] = std::max(0.0, now_ - route.probe_sent);
    if (!route.direct) {
      route.direct = packet.sender;
      status_.direct[member] = true;
      ++status_.went_direct;
      route.next_probe = now_ + direct_probe;
    }
    return true;
  } catch (const std::runtime_error &) {
    return true;
  }
}

} // namespace phantomcrash::online
