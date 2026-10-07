#pragma once

#include "phantomcrash/online_peer.hpp"
#include "phantomcrash/online_input.hpp"
#include <array>

namespace phantomcrash::online {

using Compatibility = std::array<std::uint8_t, 32>;

struct RoomOptions {
  bool host{};
  Endpoint bind;
  Endpoint server;
  Compatibility compatibility{}; // caller's executable/gameplay-content digest
  std::uint8_t capacity{3};
  std::uint32_t seed{1}; // used by host only
};

struct RoomMessage {
  std::uint8_t sender{}; // assigned by handshake, never trusted from the payload
  std::vector<std::uint8_t> bytes;
};

// Session establishment and bounded reliable messages. This owns networking,
// not the game clock. A room can stay connected while players browse menus.
class Room final {
public:
  explicit Room(RoomOptions options);
  ~Room();
  Room(const Room &) = delete;
  Room &operator=(const Room &) = delete;
  void update(double now);
  void send(std::uint8_t recipient, std::span<const std::uint8_t> bytes, double now);
  void broadcast(std::span<const std::uint8_t> bytes, double now);
  [[nodiscard]] std::size_t send_capacity(std::uint8_t recipient) const;
  [[nodiscard]] std::vector<RoomMessage> take_messages();
  [[nodiscard]] std::uint8_t local_player() const;
  [[nodiscard]] std::uint8_t participants() const;
  [[nodiscard]] std::uint32_t seed() const;
  [[nodiscard]] bool connected() const;
  [[nodiscard]] bool recovering() const;
  [[nodiscard]] Endpoint local_endpoint() const;
  [[nodiscard]] const std::string &error() const;

private:
  struct Peer {
    Endpoint endpoint;
    std::uint64_t nonce{};
    std::unique_ptr<ReliablePeer> channel;
    double heard{}, heartbeat{};
    bool ready{};
  };
  bool handshake(const Datagram &packet, double now);
  void send_handshake(Endpoint target, std::uint8_t kind, std::uint8_t slot, std::uint64_t nonce);
  void receive(Peer &peer, std::uint8_t slot, const Datagram &packet, double now);
  [[nodiscard]] std::unique_ptr<ReliablePeer> channel(Endpoint remote);
  RoomOptions options_;
  DatagramSocket socket_;
  std::uint64_t nonce_{}, token_{};
  std::uint8_t local_player_{max_players};
  std::uint8_t membership_{}; // host's participant mask, received by clients
  std::array<Peer, max_players> peers_;
  std::vector<RoomMessage> messages_;
  std::optional<double> started_, hello_;
  std::string error_;
  double now_{};
};

} // namespace phantomcrash::online
