#pragma once

#include <array>
#include <cstddef>
#include <cstdint>

// The relay's wire format, shared by its client and server. Every datagram opens
// with `magic` and a kind byte; relay_header bytes precede a forwarded payload.
namespace phantomcrash::online::relay_protocol {
constexpr std::uint32_t magic = 0x4c524350; // PCRL
constexpr std::uint8_t version = 1;

enum Kind : std::uint8_t {
  hello = 1,      // client: pads to hello_bytes, so a challenge never amplifies
  challenge = 2,  // server: proves the client receives at its claimed address
  create = 3,     // client: open a room
  join = 4,       // client: enter a room by code
  rejoin = 5,     // client: same member from a new address
  joined = 6,     // server: code, room key, member number, address seen
  refused = 7,    // server: why not
  keepalive = 8,  // client: holds the router's mapping; answered with `peers`
  peers = 9,      // server: every other member's addresses, and the echoed clock
  forward = 10,   // both: carry one game datagram to or from a member
  unknown = 11,   // server: this address is not a member (the router remapped it)
  leave = 12,     // client: free the seat now
  probe = 13,     // member to member: can this address reach you?
  probe_ack = 14, // member to member: yes
};

enum Refusal : std::uint8_t {
  no_room = 1,
  room_full = 2,
  relay_full = 3,
  too_many_tries = 4,
  bad_cookie = 5,
};

constexpr std::size_t hello_bytes = 32;
constexpr std::size_t relay_header = 8; // magic, kind, member, two reserved bytes
constexpr std::size_t code_length = 6;
constexpr std::size_t max_candidates = 4;
constexpr std::uint8_t max_members = 4;
// Without 0, O, 1 or I, which read alike when a code is passed on by voice.
constexpr std::array<char, 32> code_alphabet{'2', '3', '4', '5', '6', '7', '8', '9', 'A', 'B', 'C',
                                             'D', 'E', 'F', 'G', 'H', 'J', 'K', 'L', 'M', 'N', 'P',
                                             'Q', 'R', 'S', 'T', 'U', 'V', 'W', 'X', 'Y', 'Z'};
} // namespace phantomcrash::online::relay_protocol
