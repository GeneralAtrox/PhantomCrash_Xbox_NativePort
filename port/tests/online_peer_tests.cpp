#include "test_support.hpp"
#include "phantomcrash/online_peer.hpp"
#include <algorithm>
#include <array>

using namespace phantomcrash::online;

TEST_CASE("online peer recovers lost data and acknowledgements without duplicate delivery") {
  const Endpoint host{0x7f000001, 31001}, client{0x7f000001, 31002};
  std::vector<Datagram> to_host, to_client;
  ReliablePeer sender(client, 77, [&](Endpoint target, std::span<const std::uint8_t> bytes) {
    CHECK(target == client);
    to_client.push_back({host, {bytes.begin(), bytes.end()}});
    return true;
  });
  ReliablePeer receiver(host, 77, [&](Endpoint target, std::span<const std::uint8_t> bytes) {
    CHECK(target == host);
    to_host.push_back({client, {bytes.begin(), bytes.end()}});
    return true;
  });
  for (std::uint8_t i = 1; i <= 3; ++i) {
    sender.send(std::array<std::uint8_t, 1>{i}, 0);
  }
  CHECK(sender.pending() == 3);
  // Lose sequence 1, then deliver 3 before 2. Neither can be applied yet.
  CHECK(receiver.receive(to_client[2], 0.01).empty());
  CHECK(receiver.receive(to_client[1], 0.01).empty());
  to_client.clear();
  // Lose both acknowledgements too; all three will be retransmitted.
  to_host.clear();
  sender.update(0.06);
  CHECK(sender.retransmissions() == 3);
  std::vector<int> delivered;
  for (const auto &datagram : to_client) {
    for (const auto &message : receiver.receive(datagram, 0.07)) {
      delivered.push_back(message.at(0));
    }
    CHECK(receiver.receive(datagram, 0.08).empty());
  }
  CHECK(delivered == (std::vector<int>{1, 2, 3}));
  for (const auto &datagram : to_host) {
    CHECK(sender.receive(datagram, 0.09).empty());
  }
  CHECK(sender.pending() == 0);
}

TEST_CASE("online peer ignores other endpoints and sessions before acknowledging") {
  const Endpoint host{0x7f000001, 31001}, client{0x7f000001, 31002};
  std::vector<Datagram> outbound;
  int acknowledgements = 0;
  ReliablePeer sender(client, 77, [&](Endpoint, std::span<const std::uint8_t> bytes) {
    outbound.push_back({host, {bytes.begin(), bytes.end()}});
    return true;
  });
  ReliablePeer receiver(host, 88, [&](Endpoint, std::span<const std::uint8_t>) {
    ++acknowledgements;
    return true;
  });
  sender.send(std::array<std::uint8_t, 1>{9}, 0);
  CHECK(receiver.receive(outbound[0], 0).empty());
  CHECK(acknowledgements == 0);
  outbound[0].sender = client;
  CHECK(receiver.receive(outbound[0], 0).empty());
  CHECK(acknowledgements == 0);
}

TEST_CASE("online peer bounds unacknowledged data and reports timeout") {
  ReliablePeer peer({0x7f000001, 31001}, 1,
                    [](Endpoint, std::span<const std::uint8_t>) { return true; });
  peer.send(std::array<std::uint8_t, 1>{1}, 0);
  bool timed_out = false;
  try {
    peer.update(10);
  } catch (const std::runtime_error &) {
    timed_out = true;
  }
  CHECK(timed_out);
}
