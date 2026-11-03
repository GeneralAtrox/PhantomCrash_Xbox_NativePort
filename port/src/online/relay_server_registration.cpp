#include "relay_server_impl.hpp"

namespace phantomcrash::online {

void RelayServer::Impl::handle_registration(const Datagram &packet, WireReader &in,
                                            const std::uint8_t kind) {
  if (in.u8() != version) {
    return;
  }
  const auto who = packet.sender;
  const auto nonce = in.u64();
  if (!cookie_valid(who, in.u64())) {
    refuse(who, nonce, bad_cookie);
    return;
  }
  const bool hidden = (in.u8() & 1U) != 0;
  std::uint64_t key{};
  std::uint8_t rejoin_member{};
  std::string code;
  if (kind == rejoin) {
    key = in.u64();
    rejoin_member = in.u8();
  } else if (kind == join) {
    const auto text = in.raw(code_length);
    code.assign(text.begin(), text.end());
  }
  std::vector<Endpoint> local(std::min<std::size_t>(in.u8(), max_candidates));
  for (auto &endpoint : local) {
    endpoint.address = in.u32();
    endpoint.port = in.u16();
  }
  // A repeat of a registration whose answer was lost.
  if (const auto seat = seats.find(who); seat != seats.end()) {
    const auto room = rooms.find(seat->second.code);
    if (room != rooms.end() && room->second.members[seat->second.member].nonce == nonce) {
      send_joined(who, seat->second.code, room->second, seat->second.member);
      return;
    }
    forget_seat(who);
  }
  if (kind == create) {
    if (rooms.size() >= options.max_rooms) {
      refuse(who, nonce, relay_full);
    } else if (!spend_attempt(who)) {
      refuse(who, nonce, too_many_tries);
    } else {
      code = new_code();
      auto &room = rooms[code];
      room.key = random_u64();
      ++stats.rooms_opened;
      seat_member(room, code, 0, who, nonce, hidden, std::move(local));
    }
    return;
  }
  if (kind == join) {
    const auto room = rooms.find(code);
    if (room == rooms.end()) {
      refuse(who, nonce, spend_attempt(who) ? no_room : too_many_tries);
      return;
    }
    for (std::uint8_t member = 1; member < max_members; ++member) {
      if (!room->second.members[member].present) {
        seat_member(room->second, code, member, who, nonce, hidden, std::move(local));
        return;
      }
    }
    refuse(who, nonce, room_full);
    return;
  }
  // A member whose router gave it a new address keeps its seat, if the key matches.
  for (auto &[name, room] : rooms) {
    if (room.key == key && rejoin_member < max_members && room.members[rejoin_member].present) {
      seats.erase(room.members[rejoin_member].address);
      const auto kept = name;
      seat_member(room, kept, rejoin_member, who, nonce, hidden, std::move(local));
      return;
    }
  }
  refuse(who, nonce, spend_attempt(who) ? no_room : too_many_tries);
}

} // namespace phantomcrash::online
