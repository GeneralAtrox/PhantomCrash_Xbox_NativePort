#include "relay_server_impl.hpp"

namespace phantomcrash::online {

RelayServer::RelayServer(RelayServerOptions options) : impl_(std::make_unique<Impl>(options)) {}

RelayServer::~RelayServer() = default;

void RelayServer::pump(const double now, const double wait) {
  auto &self = *impl_;
  self.now = now;
  const double bounded = self.delayed.empty() ? wait : std::min(wait, 0.001);
  static_cast<void>(self.socket.wait_readable(bounded));
  for (std::size_t count = 0; count < receive_budget; ++count) {
    const auto packet = self.socket.receive();
    if (!packet) {
      break;
    }
    self.handle(*packet);
  }
  while (!self.delayed.empty() && self.delayed.front().due <= now) {
    static_cast<void>(self.socket.send(self.delayed.front().target, self.delayed.front().bytes));
    self.delayed.pop_front();
  }
  if (now >= self.next_sweep) {
    self.sweep();
  }
}

Endpoint RelayServer::local() const {
  return impl_->socket.local();
}

RelayServerStats RelayServer::stats() const {
  auto result = impl_->stats;
  result.rooms_open = impl_->rooms.size();
  result.members_online = impl_->seats.size();
  return result;
}

} // namespace phantomcrash::online
