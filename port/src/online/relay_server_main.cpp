#include "phantomcrash/online_relay.hpp"
#include <chrono>
#include <format>
#include <iostream>
#include <string>

// phantomcrash_relay [--port <udp>] [--seconds <limit>] [--max-rooms <count>]
//                    [--test-loss <share>] [--test-delay-ms <one way>] [--test-open <IPv4:port>]...
// Counts only: it prints no address, code or payload.
int main(const int argc, char **argv) {
  using namespace phantomcrash::online;
  try {
    RelayServerOptions options;
    options.bind.port = relay_default_port;
    double limit{};
    for (int index = 1; index < argc; ++index) {
      const std::string argument = argv[index];
      if (++index >= argc) {
        throw std::runtime_error("Missing value for " + argument);
      }
      const std::string value = argv[index];
      if (argument == "--port") {
        options.bind.port = static_cast<std::uint16_t>(std::stoi(value));
      } else if (argument == "--seconds") {
        limit = std::stod(value);
      } else if (argument == "--max-rooms") {
        options.max_rooms = static_cast<std::size_t>(std::stoul(value));
      } else if (argument == "--test-loss") {
        options.test_loss = std::stod(value);
      } else if (argument == "--test-delay-ms") {
        options.test_delay = std::stod(value) / 1000.0;
      } else if (argument == "--test-open") {
        const auto colon = value.find(':');
        options.test_open.push_back(
            Endpoint::parse(value.substr(0, colon),
                            static_cast<std::uint16_t>(std::stoi(value.substr(colon + 1)))));
      } else {
        throw std::runtime_error("Unknown option " + argument);
      }
    }
    RelayServer server(options);
    std::cout << "relay listening on UDP " << server.local().port << std::endl;
    const auto start = std::chrono::steady_clock::now();
    double next_report = 10;
    for (;;) {
      const double now =
          std::chrono::duration<double>(std::chrono::steady_clock::now() - start).count() + 1.0;
      server.pump(now, 0.05);
      if (limit > 0 && now >= limit) {
        break;
      }
      if (now >= next_report) {
        next_report += 10;
        const auto stats = server.stats();
        std::cout << std::format("t={:.0f}s rooms={} members={} opened={} forwarded={} bytes={} "
                                 "rate_dropped={} refused={}",
                                 now, stats.rooms_open, stats.members_online, stats.rooms_opened,
                                 stats.forwarded, stats.forwarded_bytes, stats.dropped_rate,
                                 stats.refused)
                  << std::endl;
      }
    }
    return 0;
  } catch (const std::exception &error) {
    std::cerr << error.what() << '\n';
    return 1;
  }
}
