#include "desktop_coop_storage.hpp"
#include "desktop_host.hpp"
#include "desktop_online_factory.hpp"
#include "desktop_online_game.hpp"
#include "desktop_online_session.hpp"
#include "desktop_tools.hpp"
#include "phantomcrash/online_content.hpp"
#include "phantomcrash/online_lobby.hpp"
#include <algorithm>
#include <chrono>
#include <cmath>
#include <cstdlib>
#include <format>
#include <iostream>
#include <map>
#include <stdexcept>
#include <thread>

// Hidden check of one side of a real online match: run it once on each of two
// computers, directly or through a relay. Each side plays the same scripted
// pilot as the stress check, with no window drawn, and prints its round trip,
// stalls and rollbacks, and the state digest at every checkpoint tick. The
// two logs agree only if both computers simulated the same match.
//
// --online-match-check role=host,relay=IP:port[,relay_only=1][,seconds=120]
// --online-match-check role=join,relay=IP:port,code=ABC234[,relay_only=1]
// --online-match-check role=host,port=29641   (and role=join,direct=IP:port)
// Tests add loss=0.02 (dropped when sent) and cut=30 (direct path dies then).

namespace phantomcrash {
namespace {
constexpr double frame_seconds = 1.0 / 144.0;
constexpr double setup_limit = 180.0, overrun_limit = 240.0; // seconds
// Hashing every content file for the room handshake takes minutes without SHA instructions.
constexpr double hash_limit = 1200.0;
constexpr double report_seconds = 10.0, linger_seconds = 5.0;
constexpr std::uint32_t checkpoint_ticks = 600;
constexpr double stall_gap = 3.0 * online::tick_seconds; // no new tick for this long
constexpr std::pair<int, int> canvas_size{1280, 720};
constexpr double aim_sway = 0.5;
constexpr long long walk_out_ticks = 600;

// Same pilot as the stress check: every remote input mispredicts and rolls back.
InputFrame pilot(const int peer, const long long tick) {
  InputFrame input;
  const double phase = static_cast<double>(tick) * 0.05 + peer;
  input.right_stick = {0.0F, static_cast<float>(aim_sway * std::sin(phase))};
  if (tick < walk_out_ticks) {
    input.left_stick = {0.0F, 1.0F};
  }
  input.held = button::right_trigger | ((tick / 90) % 2 ? button::left_trigger : 0U);
  input.ports[0].connected = true;
  input.ports[0].held = input.held;
  input.ports[0].left_stick = input.left_stick;
  input.ports[0].right_stick = input.right_stick;
  return input;
}

std::map<std::string, std::string> parse_settings(const std::string &text) {
  std::map<std::string, std::string> settings;
  std::size_t at = 0;
  while (at < text.size()) {
    const auto comma = std::min(text.find(',', at), text.size());
    const auto equals = text.find('=', at);
    if (equals == std::string::npos || equals > comma) {
      throw std::runtime_error("Match check settings are key=value,key=value");
    }
    settings[text.substr(at, equals - at)] = text.substr(equals + 1, comma - equals - 1);
    at = comma + 1;
  }
  return settings;
}

online::Endpoint parse_address(const std::string &text) {
  const auto colon = text.rfind(':');
  if (colon == std::string::npos) {
    throw std::runtime_error("Match check addresses are IPv4:port");
  }
  return online::Endpoint::parse(text.substr(0, colon),
                                 static_cast<std::uint16_t>(std::stoi(text.substr(colon + 1))));
}

std::string hex(const std::span<const std::uint8_t> bytes, const std::size_t count) {
  std::string text;
  for (std::size_t i = 0; i < std::min(count, bytes.size()); ++i) {
    text += std::format("{:02x}", bytes[i]);
  }
  return text;
}

struct Figures {
  double rtt_sum{}, rtt_min{1e9}, rtt_max{};
  int rtt_samples{};
  int stalls{};
  double stall_seconds{}, worst_gap{};
};
} // namespace

int check_online_match(const std::string &text, const DataRoots &roots,
                       const std::filesystem::path &root) {
  const auto settings = parse_settings(text);
  const auto value = [&](const char *key, const std::string &otherwise = {}) {
    const auto found = settings.find(key);
    return found == settings.end() ? otherwise : found->second;
  };
  const bool is_host = value("role") == "host";
  const double game_seconds = std::stod(value("seconds", "120"));
  // A hung run must end by itself whichever step it hung in.
  std::thread([limit = setup_limit + hash_limit + game_seconds + overrun_limit] {
    std::this_thread::sleep_for(std::chrono::duration<double>(limit));
    std::cout << "FAIL: match check overran" << std::endl;
    std::_Exit(3);
  }).detach();

  const auto setup_started = std::chrono::steady_clock::now();
  const auto progress = [&](const char *step) {
    std::cout << std::format(
                     "{} after {:.1f} s", step,
                     std::chrono::duration<double>(std::chrono::steady_clock::now() - setup_started)
                         .count())
              << std::endl;
  };
  DesktopHost host(false, true, canvas_size);
  progress("window ready");
  auto resources = std::make_shared<OnlineBattleResources>(host.renderer(), roots);
  const auto &preset = resources->rumbling.quick_rentals.at(0);
  online::BattleSetup setup;
  for (int slot = 0; slot < 2; ++slot) {
    online::BattlePlayer player;
    player.profile.player_name = "Match player " + std::to_string(slot + 1);
    player.profile.money = 1000000;
    const auto entry = prepare_quick_rental(preset, resources->contests, resources->rules,
                                            *resources->garage, player.profile);
    if (!entry) {
      throw std::runtime_error("Rental entry failed");
    }
    player.entry = *entry;
    setup.players[slot] = std::move(player);
  }
  setup.world = setup.players[0]->profile;
  setup.area = setup.players[0]->entry.area;
  setup.contest = setup.players[0]->entry.contest;
  resources.reset();
  progress("battle data loaded");

  online::RoomOptions room_options;
  room_options.host = is_host;
  room_options.bind = {0, static_cast<std::uint16_t>(std::stoi(value("port", "0")))};
  room_options.compatibility = online::content_fingerprint(running_executable(), root, false);
  progress("content hashed");
  std::cout << "content " << hex(room_options.compatibility, 8) << std::endl;
  if (!value("relay").empty()) {
    online::RelayOptions relay;
    relay.server = parse_address(value("relay"));
    relay.code = value("code");
    relay.relay_only = value("relay_only") == "1";
    relay.test_loss = std::stod(value("loss", "0"));
    relay.test_cut_at = std::stod(value("cut", "0"));
    room_options.relay = relay;
  } else if (!is_host) {
    room_options.server = parse_address(value("direct"));
  }
  online::Lobby lobby(room_options);
  auto &room = lobby.room();
  lobby.start_transport_worker();

  using Clock = std::chrono::steady_clock;
  const auto now_seconds = [] {
    return std::chrono::duration<double>(Clock::now().time_since_epoch()).count();
  };
  auto matches = std::make_shared<CoopMatches>();
  std::optional<OnlineGame> game;
  const OnlineGame::Key key{setup.world.day, setup.area, setup.contest};
  const auto pump = [&] {
    const double now = now_seconds();
    lobby.update(now);
    if (!room.error().empty()) {
      throw std::runtime_error("Room failed: " + room.error());
    }
    if (game) {
      for (const auto &message : lobby.take_messages()) {
        static_cast<void>(game->receive(message, now));
      }
      game->update(now);
    }
  };
  const auto wait_for = [&](const auto &condition, const char *what) {
    const double limit = now_seconds() + setup_limit;
    while (!condition()) {
      if (now_seconds() >= limit) {
        throw std::runtime_error(std::string("Timed out waiting for ") + what);
      }
      pump();
      std::this_thread::sleep_for(std::chrono::milliseconds(1));
    }
  };

  const double started_waiting = now_seconds();
  if (room_options.relay) {
    wait_for([&] { return !room.room_code().empty(); }, "the relay");
    std::cout << "ROOM " << room.room_code() << std::endl;
  }
  wait_for([&] { return room.connected() && room.participants() == 3; }, "the other computer");
  const int local = room.local_player();
  const int other = 1 - local;
  std::cout << std::format("connected as player {} after {:.2f} s", local + 1,
                           now_seconds() - started_waiting)
            << std::endl;
  game.emplace(room, host.renderer(), roots, matches, setup.options, setup.robots);
  const auto enter = [&](const int entrant) {
    if (entrant == local) {
      game->request_entry(setup.players[entrant]->profile, setup.players[entrant]->entry);
    }
    wait_for([&] { return game->ready(key, entrant); }, "the arena admission");
  };
  enter(0);
  enter(1);
  const auto match = game->match(key);
  match->battle->set_cheats({true, true}); // before this computer's first tick
  auto view = make_online_battle_view(match, local, {});
  auto &session = *match->online;
  std::cout << "in battle" << std::endl;

  const auto end_tick = static_cast<std::uint32_t>(
      std::ceil(game_seconds / online::tick_seconds / checkpoint_ticks) * checkpoint_ticks);
  std::uint32_t next_checkpoint = checkpoint_ticks;
  Figures figures;
  const double play_start = now_seconds();
  double frame_at = play_start, carry = 0, next_report = play_start + report_seconds;
  double next_sample = play_start + 1.0, last_tick_time = play_start;
  double finished_at = 0;
  std::uint32_t last_tick = session.next_tick();
  long long ticks = 0;
  bool pumped = false;
  bool agreed_digests = true;
  // A confirmed state is kept only while a rollback might need it, so read it
  // while it is current (inputs early) or still held (inputs late).
  const auto take_checkpoint = [&] {
    if (session.confirmed_tick() < next_checkpoint) {
      return;
    }
    const auto digest = session.confirmed_digest(next_checkpoint);
    std::cout << "DIGEST " << next_checkpoint << " " << (digest ? hex(*digest, 16) : "none")
              << std::endl;
    agreed_digests &= digest.has_value();
    next_checkpoint += checkpoint_ticks;
  };
  for (;;) {
    const double now = now_seconds();
    carry += std::min(now - frame_at, 0.1);
    frame_at = now;
    const int due = static_cast<int>(carry / online::tick_seconds);
    carry -= due * online::tick_seconds;
    if (due == 0) {
      pump();
      pumped = true;
    }
    for (int tick = 0; tick < due; ++tick) {
      if (!pumped) {
        pump();
      }
      pumped = false;
      if (view && !view->done()) {
        view->tick(pilot(local, ticks + tick), online::tick_seconds);
      } else if (session.needs_input()) {
        view.reset();
        session.capture({}, {}, std::nullopt, now);
      }
      take_checkpoint();
      game->advance();
    }
    ticks += due;
    if (session.next_tick() != last_tick) {
      const double gap = now - last_tick_time;
      if (gap > stall_gap) {
        ++figures.stalls;
        figures.stall_seconds += gap;
      }
      figures.worst_gap = std::max(figures.worst_gap, gap);
      last_tick = session.next_tick();
      last_tick_time = now;
    }
    take_checkpoint();
    if (now >= next_sample) {
      next_sample += 1.0;
      const double rtt = room.round_trip(static_cast<std::uint8_t>(other)) * 1000.0;
      if (rtt > 0) {
        figures.rtt_sum += rtt;
        figures.rtt_min = std::min(figures.rtt_min, rtt);
        figures.rtt_max = std::max(figures.rtt_max, rtt);
        ++figures.rtt_samples;
      }
    }
    if (now >= next_report) {
      next_report += report_seconds;
      const auto status = room.relay_status();
      std::cout << std::format(
                       "t={:.0f}s tick={} confirmed={} rollbacks={} rtt={:.1f}ms stalls={} {}",
                       now - play_start, session.next_tick(), session.confirmed_tick(),
                       session.rollback_count(),
                       room.round_trip(static_cast<std::uint8_t>(other)) * 1000.0, figures.stalls,
                       status ? std::format("route={} relayed={} direct={}",
                                            status->direct[other] ? "direct" : "relay",
                                            status->sent_relayed, status->sent_direct)
                              : std::string("route=address"))
                << std::endl;
    }
    if (!finished_at && session.confirmed_tick() >= end_tick) {
      finished_at = now;
    }
    if (finished_at && now - finished_at >= linger_seconds) {
      break; // the other computer needs this one's inputs to reach the same tick
    }
    const double wake = frame_at + frame_seconds;
    while (now_seconds() < wake) {
      room.wait(std::max(0.0, std::min(wake - now_seconds(), 0.001)));
    }
  }
  const double elapsed = finished_at - play_start;
  const auto status = room.relay_status();
  std::cout << std::format(
                   "RESULT tick_rate={:.2f}/s rollbacks={} rollbacks_per_s={:.1f} rtt_mean={:.1f} "
                   "rtt_min={:.1f} rtt_max={:.1f} stalls={} stall_s={:.2f} worst_gap_ms={:.0f}",
                   end_tick / elapsed, session.rollback_count(),
                   static_cast<double>(session.rollback_count()) / elapsed,
                   figures.rtt_samples ? figures.rtt_sum / figures.rtt_samples : 0.0,
                   figures.rtt_samples ? figures.rtt_min : 0.0, figures.rtt_max, figures.stalls,
                   figures.stall_seconds, figures.worst_gap * 1000.0)
            << std::endl;
  if (status) {
    std::cout << std::format("ROUTE sent_relayed={} sent_direct={} received_relayed={} "
                             "received_direct={} went_direct={} fell_back={} server_rtt_ms={:.1f} "
                             "direct_rtt_ms={:.1f}",
                             status->sent_relayed, status->sent_direct, status->received_relayed,
                             status->received_direct, status->went_direct, status->fell_back,
                             status->server_round_trip * 1000.0,
                             status->direct_round_trip[other] * 1000.0)
              << std::endl;
  }
  std::cout << (agreed_digests ? "PASS" : "FAIL") << ": played to tick " << end_tick << std::endl;
  return agreed_digests ? 0 : 1;
}

} // namespace phantomcrash
