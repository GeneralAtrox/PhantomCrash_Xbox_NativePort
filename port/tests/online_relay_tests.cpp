#include "test_support.hpp"
#include "phantomcrash/online_relay.hpp"
#include "phantomcrash/online_room.hpp"
#include <chrono>
#include <thread>

using namespace phantomcrash::online;

namespace {
struct Rig {
  RelayServer server;
  double now{1};
  Rig() : server(RelayServerOptions{Endpoint::parse("127.0.0.1", 0)}) {}
  void pump(std::initializer_list<Room *> rooms, const int passes = 40) {
    for (int pass = 0; pass < passes; ++pass) {
      server.pump(now, 0);
      for (auto *room : rooms) {
        room->update(now);
      }
      server.pump(now, 0);
      now += 0.01;
      std::this_thread::sleep_for(std::chrono::milliseconds(1));
    }
  }
  [[nodiscard]] RoomOptions options(const std::string &code, const bool relay_only) const {
    RoomOptions room;
    room.host = code.empty();
    room.bind = Endpoint::parse("127.0.0.1", 0);
    room.compatibility[0] = 7;
    room.relay = RelayOptions{server.local(), code, relay_only};
    return room;
  }
};
} // namespace

TEST_CASE("online room plays through the relay alone with room and digest unchanged") {
  Rig rig;
  Room host(rig.options({}, true));
  rig.pump({&host});
  const auto code = host.room_code();
  REQUIRE(code.size() == 6);
  Room client(rig.options(code, true));
  rig.pump({&host, &client}, 60);
  CHECK(host.error().empty());
  CHECK(client.error().empty());
  REQUIRE(client.connected());
  CHECK(client.local_player() == 1);
  CHECK(host.participants() == 3);
  // A datagram of the largest size the game may send crosses with the frame on top.
  const std::vector<std::uint8_t> payload(max_peer_payload - 1, 42);
  client.send(0, payload, rig.now);
  rig.pump({&host, &client});
  const auto messages = host.take_messages();
  REQUIRE(messages.size() == 1);
  CHECK(messages[0].sender == 1);
  CHECK(messages[0].bytes == payload);
  const auto status = client.relay_status();
  REQUIRE(status.has_value());
  CHECK(status->sent_relayed > 0);
  CHECK(status->sent_direct == 0);
  CHECK(rig.server.stats().forwarded > 0);
}

TEST_CASE("online relay refuses a different game build through the room's own handshake") {
  Rig rig;
  Room host(rig.options({}, true));
  rig.pump({&host});
  auto options = rig.options(host.room_code(), true);
  options.compatibility[0] = 8;
  Room client(options);
  rig.pump({&host, &client}, 60);
  CHECK(host.participants() != 3);
  CHECK(client.error().find("different game build") != std::string::npos);
}

TEST_CASE("online relay upgrades to a direct path and falls back when it dies") {
  Rig rig;
  auto host_options = rig.options({}, false);
  host_options.relay->test_cut_at = 3.0;
  Room host(host_options);
  rig.pump({&host});
  Room client(rig.options(host.room_code(), false));
  rig.pump({&host, &client}, 100);
  REQUIRE(client.connected());
  REQUIRE(client.relay_status()->direct[0]);
  CHECK(host.relay_status()->direct[1]);
  const auto relayed = rig.server.stats().forwarded;
  const std::array<std::uint8_t, 3> first{1, 2, 3};
  client.send(0, first, rig.now);
  rig.pump({&host, &client});
  CHECK(host.take_messages().size() == 1);
  CHECK(rig.server.stats().forwarded == relayed);
  // Past the cut the same room carries on through the relay, with no reconnect.
  rig.pump({&host, &client}, 400);
  CHECK(host.relay_status()->fell_back == 1);
  const std::array<std::uint8_t, 3> second{4, 5, 6};
  host.send(1, second, rig.now);
  rig.pump({&host, &client}, 400);
  const auto messages = client.take_messages();
  REQUIRE(!messages.empty());
  CHECK((messages.back().bytes == std::vector<std::uint8_t>{4, 5, 6}));
  CHECK(host.error().empty());
  CHECK(client.error().empty());
  CHECK(rig.server.stats().forwarded > relayed);
}

TEST_CASE("online relay refuses a wrong code and a full room") {
  Rig rig;
  Room wrong(rig.options("ZZZZZZ", true));
  rig.pump({&wrong});
  CHECK(wrong.error() == "No room has that code");
  Room host(rig.options({}, true));
  rig.pump({&host});
  const auto code = host.room_code();
  Room a(rig.options(code, true)), b(rig.options(code, true)), c(rig.options(code, true));
  rig.pump({&host, &a, &b, &c}, 60);
  Room extra(rig.options(code, true));
  rig.pump({&host, &a, &b, &c, &extra}, 60);
  CHECK(extra.error() == "That room is full");
}

TEST_CASE("online relay ignores registration without the challenge cookie") {
  Rig rig;
  Room host(rig.options({}, true));
  rig.pump({&host});
  DatagramSocket attacker(Endpoint::parse("127.0.0.1", 0));
  const std::array<std::uint8_t, 8> frame{0x50, 0x43, 0x52, 0x4c, 3, 1, 0, 0};
  static_cast<void>(attacker.send(rig.server.local(), frame));
  rig.pump({&host});
  CHECK(rig.server.stats().rooms_open == 1);
}
