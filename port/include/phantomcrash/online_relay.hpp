#pragma once

#include "phantomcrash/online_socket.hpp"
#include <array>
#include <map>
#include <random>

namespace phantomcrash::online {

inline constexpr std::uint16_t relay_default_port = 29650;

struct RelayOptions {
  Endpoint server;
  std::string code;     // empty: open a room, and the relay names it
  bool relay_only{};    // never try a direct path; other players never see this address
  double test_loss{};   // test only: this share of game datagrams sent are dropped
  double test_cut_at{}; // test only: seconds after which direct paths stop working
};

struct RelayStatus {
  std::string code;
  std::string error;
  bool joined{};
  std::uint8_t member{};
  Endpoint observed; // this socket's address as the relay sees it
  std::uint64_t sent_relayed{}, sent_direct{}, received_relayed{}, received_direct{};
  std::uint64_t went_direct{}, fell_back{};
  double server_round_trip{}; // seconds; zero until measured
  std::array<bool, 4> direct{};
  std::array<double, 4> direct_round_trip{};
};

// Makes one UDP socket reach the other members of a room through the relay,
// and straight to them where the routers allow it, under stable made-up
// addresses. The room above never learns which path a datagram took.
class RelayClient final {
public:
  RelayClient(RelayOptions options, DatagramSocket &socket);
  [[nodiscard]] static Endpoint address_of_member(std::uint8_t member);
  void update(double now);
  bool send(Endpoint target, std::span<const std::uint8_t> bytes);
  // The next game datagram; relay and probe traffic are handled on the way.
  [[nodiscard]] std::optional<Datagram> receive();
  void leave();
  [[nodiscard]] const RelayStatus &status() const {
    return status_;
  }

private:
  struct Route {
    bool present{};
    std::vector<Endpoint> candidates;
    std::optional<Endpoint> direct;
    std::uint64_t probe_nonce{};
    double probe_sent{}, next_probe{}, heard{}, first_seen{};
  };
  void send_hello(double now);
  void send_registration();
  void send_keepalive(double now);
  void send_probes(double now);
  void handle_server(const Datagram &packet, std::optional<Datagram> &game);
  void handle_peers(std::span<const std::uint8_t> bytes);
  [[nodiscard]] bool handle_probe(const Datagram &packet);
  [[nodiscard]] bool direct_blocked() const;
  [[nodiscard]] Route *route_of(Endpoint target);

  RelayOptions options_;
  DatagramSocket &socket_;
  RelayStatus status_;
  std::array<Route, 4> routes_;
  std::map<Endpoint, std::uint8_t> known_; // direct senders proven by the room key
  std::vector<Endpoint> local_;
  std::minstd_rand random_;
  std::uint64_t nonce_{}, cookie_{}, room_key_{};
  double now_{}, started_{}, next_hello_{}, next_keepalive_{}, heard_server_{};
  bool have_cookie_{}, rejoining_{};
};

struct RelayServerOptions {
  Endpoint bind;
  std::size_t max_rooms{1000};
  std::size_t member_bytes_per_second{256 * 1024}; // beyond this a member's datagrams drop
  double test_loss{};                              // share of forwarded datagrams dropped
  double test_delay{};                             // seconds added to each forwarded datagram
  // Test only: sent a small datagram each second, so a firewall on the relay's
  // own computer admits their replies as it would on an open server.
  std::vector<Endpoint> test_open;
};

struct RelayServerStats {
  std::uint64_t rooms_opened{}, forwarded{}, forwarded_bytes{}, dropped_rate{}, refused{};
  std::size_t rooms_open{}, members_online{};
};

// The relay itself: names rooms, tells members each other's addresses and
// forwards their datagrams. It reads no payload and logs no address.
class RelayServer final {
public:
  explicit RelayServer(RelayServerOptions options);
  ~RelayServer();
  RelayServer(const RelayServer &) = delete;
  RelayServer &operator=(const RelayServer &) = delete;
  // Handles what is waiting, up to a fixed budget. Returns after `wait` at most.
  void pump(double now, double wait);
  [[nodiscard]] Endpoint local() const;
  [[nodiscard]] RelayServerStats stats() const;

private:
  struct Impl;
  std::unique_ptr<Impl> impl_;
};

// All the IPv4 addresses of this computer's active network adapters.
[[nodiscard]] std::vector<std::uint32_t> local_addresses();

} // namespace phantomcrash::online
