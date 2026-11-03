#include "phantomcrash/online_relay.hpp"
#include "relay_protocol.hpp"
#include <algorithm>
#include <stdexcept>

namespace phantomcrash::online {
using namespace relay_protocol;

namespace {
constexpr double register_interval = 0.25, register_timeout = 10.0;
constexpr double keepalive_fast = 1.0, keepalive_slow = 5.0;
constexpr std::uint32_t member_address_base = 0xf0000000U; // reserved, never a real sender
constexpr std::size_t receive_budget = 64;

} // namespace

RelayClient::RelayClient(RelayOptions options, DatagramSocket &socket)
    : options_(std::move(options)), socket_(socket),
      random_(static_cast<std::uint32_t>(random_u64())), nonce_(random_u64()) {
  if (options_.server.port == 0) {
    throw std::runtime_error("The relay server needs an address and port");
  }
  if (!options_.code.empty() && options_.code.size() != code_length) {
    throw std::runtime_error("A room code has six characters");
  }
  const auto bound = socket_.local();
  if (bound.address) {
    local_.push_back(bound);
  } else if (!options_.relay_only) {
    for (const auto address : local_addresses()) {
      if (local_.size() + 1 < max_candidates) {
        local_.push_back({address, bound.port});
      }
    }
  }
}

Endpoint RelayClient::address_of_member(const std::uint8_t member) {
  return {member_address_base | member, 1};
}

RelayClient::Route *RelayClient::route_of(const Endpoint target) {
  const auto member = target.address ^ member_address_base;
  return target.port == 1 && member < routes_.size() ? &routes_[member] : nullptr;
}

bool RelayClient::direct_blocked() const {
  return options_.test_cut_at > 0.0 && now_ - started_ >= options_.test_cut_at;
}

void RelayClient::send_hello(const double now) {
  WireWriter out;
  open(out, hello);
  out.u8(version);
  out.u64(nonce_);
  auto bytes = out.take();
  bytes.resize(hello_bytes); // as large as what it asks the relay to send back
  static_cast<void>(socket_.send(options_.server, bytes));
  next_hello_ = now + register_interval;
}

void RelayClient::send_registration() {
  WireWriter out;
  const bool rejoin_room = rejoining_ && room_key_ != 0;
  open(out, rejoin_room ? rejoin : options_.code.empty() ? create : join);
  out.u8(version);
  out.u64(nonce_);
  out.u64(cookie_);
  out.u8(options_.relay_only ? 1 : 0);
  if (rejoin_room) {
    out.u64(room_key_);
    out.u8(status_.member);
  } else if (!options_.code.empty()) {
    out.raw({reinterpret_cast<const std::uint8_t *>(options_.code.data()), code_length});
  }
  out.u8(static_cast<std::uint8_t>(local_.size()));
  for (const auto endpoint : local_) {
    write_endpoint(out, endpoint);
  }
  static_cast<void>(socket_.send(options_.server, out.take()));
}

void RelayClient::send_keepalive(const double now) {
  WireWriter out;
  open(out, keepalive);
  out.u64(room_key_);
  out.u8(status_.member);
  out.u64(static_cast<std::uint64_t>(now * 1e6));
  static_cast<void>(socket_.send(options_.server, out.take()));
  const bool alone = std::ranges::none_of(routes_, &Route::present);
  next_keepalive_ = now + (alone ? keepalive_fast : keepalive_slow);
}

void RelayClient::update(const double now) {
  if (!started_) {
    started_ = now;
    heard_server_ = now;
  }
  now_ = now;
  if (!status_.error.empty()) {
    return;
  }
  if (!status_.joined || rejoining_) {
    if (!status_.joined && now - started_ >= register_timeout) {
      status_.error = "Could not reach the relay server";
      return;
    }
    if (now >= next_hello_) {
      if (have_cookie_) {
        send_registration();
        next_hello_ = now + register_interval;
      } else {
        send_hello(now);
      }
    }
    return;
  }
  if (now >= next_keepalive_) {
    send_keepalive(now);
  }
  if (!options_.relay_only) {
    send_probes(now);
  }
}

bool RelayClient::send(const Endpoint target, const std::span<const std::uint8_t> bytes) {
  auto *route = route_of(target);
  if (!route || !status_.joined) {
    return true; // like any datagram lost before the path exists; its sender repeats it
  }
  if (options_.test_loss > 0.0 &&
      std::uniform_real_distribution<double>(0.0, 1.0)(random_) < options_.test_loss) {
    return true;
  }
  if (route->direct) {
    ++status_.sent_direct;
    return direct_blocked() || socket_.send(*route->direct, bytes);
  }
  ++status_.sent_relayed;
  WireWriter out;
  open(out, forward);
  out.u8(static_cast<std::uint8_t>(target.address ^ member_address_base));
  out.u16(0);
  out.raw(bytes);
  return socket_.send(options_.server, out.take());
}

void RelayClient::handle_peers(const std::span<const std::uint8_t> bytes) {
  WireReader in(bytes);
  static_cast<void>(in.u32());
  static_cast<void>(in.u8());
  const auto echo = in.u64();
  if (echo) {
    status_.server_round_trip = std::max(0.0, now_ - static_cast<double>(echo) / 1e6);
  }
  status_.observed = read_endpoint(in);
  const auto count = in.u8();
  std::array<bool, routes_.size()> listed{};
  for (std::uint8_t entry = 0; entry < count; ++entry) {
    const auto member = in.u8();
    if (member >= routes_.size()) {
      throw std::runtime_error("Relay named an impossible member");
    }
    std::vector<Endpoint> candidates(in.u8());
    for (auto &candidate : candidates) {
      candidate = read_endpoint(in);
    }
    auto &route = routes_[member];
    listed[member] = true;
    if (!route.present) {
      route.present = true;
      route.first_seen = now_;
      route.next_probe = now_;
    }
    if (route.candidates != candidates) {
      // A new address (the router remapped it) is worth trying at the fast rate.
      route.first_seen = now_;
      route.next_probe = now_;
      route.candidates = std::move(candidates);
    }
  }
  in.finish();
  for (std::uint8_t member = 0; member < routes_.size(); ++member) {
    if (!listed[member] && routes_[member].present) {
      std::erase_if(known_, [&](const auto &entry) { return entry.second == member; });
      routes_[member] = {};
      status_.direct[member] = false;
    }
  }
}

void RelayClient::handle_server(const Datagram &packet, std::optional<Datagram> &game) {
  try {
    WireReader in(packet.bytes);
    if (in.u32() != magic) {
      return;
    }
    heard_server_ = now_;
    const auto kind = in.u8();
    if (kind == challenge) {
      if (in.u64() != nonce_) {
        return;
      }
      cookie_ = in.u64();
      in.finish();
      have_cookie_ = true;
      send_registration();
      next_hello_ = now_ + register_interval;
    } else if (kind == joined) {
      if (in.u64() != nonce_) {
        return;
      }
      room_key_ = in.u64();
      const auto code = in.raw(code_length);
      status_.member = in.u8();
      status_.observed = read_endpoint(in);
      in.finish();
      status_.code.assign(code.begin(), code.end());
      status_.joined = true;
      rejoining_ = false;
      next_keepalive_ = now_;
    } else if (kind == refused) {
      if (in.u64() != nonce_) {
        return;
      }
      const auto reason = in.u8();
      if (reason == bad_cookie) {
        have_cookie_ = false;
        return;
      }
      status_.error = reason == no_room          ? "No room has that code"
                      : reason == room_full      ? "That room is full"
                      : reason == relay_full     ? "The relay server has no room to spare"
                      : reason == too_many_tries ? "Too many wrong room codes; wait a minute"
                                                 : "The relay server refused";
    } else if (kind == peers) {
      handle_peers(packet.bytes);
    } else if (kind == forward) {
      const auto member = in.u8();
      static_cast<void>(in.u16());
      if (member < routes_.size() && packet.bytes.size() > relay_header) {
        ++status_.received_relayed;
        game = Datagram{address_of_member(member),
                        {packet.bytes.begin() + relay_header, packet.bytes.end()}};
      }
    } else if (kind == unknown && status_.joined) {
      rejoining_ = true;
      have_cookie_ = false;
      next_hello_ = now_;
    }
  } catch (const std::runtime_error &) {
    // A malformed relay datagram is one lost datagram.
  }
}

std::optional<Datagram> RelayClient::receive() {
  for (std::size_t count = 0; count < receive_budget; ++count) {
    auto packet = socket_.receive();
    if (!packet) {
      return std::nullopt;
    }
    if (packet->sender == options_.server) {
      std::optional<Datagram> game;
      handle_server(*packet, game);
      if (game) {
        return game;
      }
      continue;
    }
    if (handle_probe(*packet)) {
      continue;
    }
    const auto found = known_.find(packet->sender);
    if (found != known_.end() && !direct_blocked()) {
      ++status_.received_direct;
      packet->sender = address_of_member(found->second);
      return packet;
    }
  }
  return std::nullopt;
}

void RelayClient::leave() {
  if (!status_.joined) {
    return;
  }
  WireWriter out;
  open(out, relay_protocol::leave);
  out.u64(room_key_);
  out.u8(status_.member);
  static_cast<void>(socket_.send(options_.server, out.take()));
  status_.joined = false;
}

} // namespace phantomcrash::online
