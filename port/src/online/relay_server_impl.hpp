#pragma once

#include "../extract/sha256.hpp"
#include "phantomcrash/online_relay.hpp"
#include "relay_protocol.hpp"
#include <algorithm>
#include <charconv>
#include <deque>
#include <map>
#include <stdexcept>

// The relay server's state, private to its implementation files.
namespace phantomcrash::online {
using namespace relay_protocol;
using extract::sha256_hex;

namespace relay_server_limits {
constexpr double cookie_bucket_seconds = 60, member_expiry = 60, attempt_window = 60;
constexpr int attempts_per_window = 20; // room openings and wrong codes, per IPv4 address
constexpr std::size_t receive_budget = 256;

} // namespace relay_server_limits
using namespace relay_server_limits;

struct RelayServer::Impl {
  struct Member {
    bool present{}, hidden{};
    Endpoint address;
    std::uint64_t nonce{};
    std::vector<Endpoint> candidates;
    double heard{}, window{};
    std::size_t window_bytes{};
  };
  struct OpenRoom {
    std::uint64_t key{};
    std::array<Member, max_members> members;
  };
  struct Seat {
    std::string code;
    std::uint8_t member{};
  };
  struct Attempts {
    double window{};
    int count{};
  };
  struct Delayed {
    double due{};
    Endpoint target;
    std::vector<std::uint8_t> bytes;
  };

  explicit Impl(RelayServerOptions value)
      : options(value), socket(value.bind, relay_header), secret(random_u64()),
        random(static_cast<std::uint32_t>(random_u64())) {}

  RelayServerOptions options;
  DatagramSocket socket;
  std::uint64_t secret{};
  std::minstd_rand random;
  std::map<std::string, OpenRoom> rooms;
  std::map<Endpoint, Seat> seats;
  std::map<std::uint32_t, Attempts> attempts;
  std::deque<Delayed> delayed;
  RelayServerStats stats;
  double now{}, next_sweep{};

  [[nodiscard]] std::uint64_t cookie(const Endpoint who, const std::uint64_t bucket) const {
    WireWriter out;
    out.u64(secret);
    write_endpoint(out, who);
    out.u64(bucket);
    const auto bytes = out.take();
    const auto hex = sha256_hex(bytes);
    std::uint64_t value{};
    std::from_chars(hex.data(), hex.data() + 16, value, 16);
    return value;
  }

  [[nodiscard]] bool cookie_valid(const Endpoint who, const std::uint64_t value) const {
    const auto bucket = static_cast<std::uint64_t>(now / cookie_bucket_seconds);
    return value == cookie(who, bucket) || (bucket && value == cookie(who, bucket - 1));
  }

  // True while this address may still spend an attempt this minute.
  bool spend_attempt(const Endpoint who) {
    auto &entry = attempts[who.address];
    if (now - entry.window >= attempt_window) {
      entry = {now, 0};
    }
    return ++entry.count <= attempts_per_window;
  }

  void refuse(const Endpoint who, const std::uint64_t nonce, const Refusal reason) {
    ++stats.refused;
    WireWriter out;
    open(out, refused);
    out.u64(nonce);
    out.u8(reason);
    static_cast<void>(socket.send(who, out.take()));
  }

  void send_joined(const Endpoint who, const std::string &code, const OpenRoom &room,
                   const std::uint8_t member) {
    WireWriter out;
    open(out, joined);
    out.u64(room.members[member].nonce);
    out.u64(room.key);
    out.raw({reinterpret_cast<const std::uint8_t *>(code.data()), code_length});
    out.u8(member);
    write_endpoint(out, who);
    static_cast<void>(socket.send(who, out.take()));
  }

  void send_peers(const OpenRoom &room, const std::uint8_t member, const std::uint64_t echo) {
    WireWriter out;
    open(out, peers);
    out.u64(echo);
    write_endpoint(out, room.members[member].address);
    std::uint8_t count{};
    for (std::uint8_t other = 0; other < max_members; ++other) {
      count += other != member && room.members[other].present ? 1 : 0;
    }
    out.u8(count);
    for (std::uint8_t other = 0; other < max_members; ++other) {
      const auto &peer = room.members[other];
      if (other == member || !peer.present) {
        continue;
      }
      out.u8(other);
      if (peer.hidden || room.members[member].hidden) {
        out.u8(0);
        continue;
      }
      out.u8(static_cast<std::uint8_t>(peer.candidates.size()));
      for (const auto candidate : peer.candidates) {
        write_endpoint(out, candidate);
      }
    }
    static_cast<void>(socket.send(room.members[member].address, out.take()));
  }

  void tell_everyone(const OpenRoom &room) {
    for (std::uint8_t member = 0; member < max_members; ++member) {
      if (room.members[member].present) {
        send_peers(room, member, 0);
      }
    }
  }

  void forget_seat(const Endpoint who) {
    const auto seat = seats.find(who);
    if (seat == seats.end()) {
      return;
    }
    const auto room = rooms.find(seat->second.code);
    if (room != rooms.end()) {
      room->second.members[seat->second.member] = {};
      if (std::ranges::none_of(room->second.members, &Member::present)) {
        rooms.erase(room);
      } else {
        tell_everyone(room->second);
      }
    }
    seats.erase(seat);
  }

  [[nodiscard]] std::string new_code() {
    std::random_device source;
    for (;;) {
      std::string code(code_length, ' ');
      for (auto &character : code) {
        character = code_alphabet[source() % code_alphabet.size()];
      }
      if (!rooms.contains(code)) {
        return code;
      }
    }
  }

  void seat_member(OpenRoom &room, const std::string &code, const std::uint8_t member,
                   const Endpoint who, const std::uint64_t nonce, const bool hidden,
                   std::vector<Endpoint> local) {
    auto &seat = room.members[member];
    seat = {};
    seat.present = true;
    seat.hidden = hidden;
    seat.address = who;
    seat.nonce = nonce;
    seat.heard = now;
    seat.window = now;
    seat.candidates.push_back(who);
    for (const auto endpoint : local) {
      if (seat.candidates.size() < max_candidates && endpoint != who) {
        seat.candidates.push_back(endpoint);
      }
    }
    seats[who] = {code, member};
    send_joined(who, code, room, member);
    tell_everyone(room);
  }

  void handle_hello(const Datagram &packet, WireReader &in) {
    if (packet.bytes.size() < hello_bytes || in.u8() != version) {
      return;
    }
    const auto nonce = in.u64();
    WireWriter out;
    open(out, challenge);
    out.u64(nonce);
    out.u64(cookie(packet.sender, static_cast<std::uint64_t>(now / cookie_bucket_seconds)));
    static_cast<void>(socket.send(packet.sender, out.take()));
  }

  void handle_registration(const Datagram &packet, WireReader &in, std::uint8_t kind);

  void say_unknown(const Endpoint who) {
    WireWriter out;
    open(out, unknown);
    static_cast<void>(socket.send(who, out.take()));
  }

  void handle_member(const Datagram &packet, WireReader &in, const std::uint8_t kind) {
    const auto seat = seats.find(packet.sender);
    if (seat == seats.end()) {
      say_unknown(packet.sender);
      return;
    }
    auto &room = rooms.at(seat->second.code);
    const auto from = seat->second.member;
    auto &member = room.members[from];
    member.heard = now;
    if (kind == keepalive || kind == leave) {
      const auto key = in.u64();
      if (key != room.key || in.u8() != from) {
        return;
      }
      if (kind == keepalive) {
        send_peers(room, from, in.u64());
      } else {
        forget_seat(packet.sender);
      }
      return;
    }
    const auto to = in.u8();
    if (to >= max_members || !room.members[to].present || packet.bytes.size() <= relay_header) {
      return;
    }
    if (now - member.window >= 1.0) {
      member.window = now;
      member.window_bytes = 0;
    }
    member.window_bytes += packet.bytes.size();
    if (member.window_bytes > options.member_bytes_per_second) {
      ++stats.dropped_rate;
      return;
    }
    if (options.test_loss > 0.0 &&
        std::uniform_real_distribution<double>(0.0, 1.0)(random) < options.test_loss) {
      return;
    }
    auto bytes = packet.bytes;
    bytes[5] = from;
    ++stats.forwarded;
    stats.forwarded_bytes += bytes.size();
    const auto target = room.members[to].address;
    if (options.test_delay > 0.0) {
      delayed.push_back({now + options.test_delay, target, std::move(bytes)});
    } else {
      static_cast<void>(socket.send(target, bytes));
    }
  }

  void handle(const Datagram &packet) {
    try {
      WireReader in(packet.bytes);
      if (packet.bytes.size() < 5 || in.u32() != magic) {
        return;
      }
      const auto kind = in.u8();
      if (kind == hello) {
        handle_hello(packet, in);
      } else if (kind == create || kind == join || kind == rejoin) {
        handle_registration(packet, in, kind);
      } else if (kind == keepalive || kind == forward || kind == leave) {
        handle_member(packet, in, kind);
      }
    } catch (const std::runtime_error &) {
      // A malformed datagram costs its sender one datagram.
    }
  }

  void sweep() {
    next_sweep = now + 1.0;
    std::vector<Endpoint> silent;
    for (const auto &[who, seat] : seats) {
      if (now - rooms.at(seat.code).members[seat.member].heard >= member_expiry) {
        silent.push_back(who);
      }
    }
    for (const auto who : silent) {
      forget_seat(who);
    }
    std::erase_if(attempts,
                  [&](const auto &entry) { return now - entry.second.window >= attempt_window; });
  }
};

} // namespace phantomcrash::online
