#include "desktop_online_presentation.hpp"
#include <algorithm>
#include <stdexcept>

namespace phantomcrash::online {
void Presentation::collect(CoopMatch &match, Prediction &prediction) {
  const auto end = std::min(prediction.next_tick(), prediction.confirmed_tick());
  while (next_ <= end) {
    if (frames_.size() >= 120) {
      throw std::runtime_error("Confirmed presentation fell behind its bounded queue");
    }
    const auto tick = static_cast<std::uint32_t>(next_);
    if (!prediction.inspect_confirmed(tick,
                                      [&] { frames_.push_back({tick, match.checkpoint()}); })) {
      throw std::runtime_error("Confirmed presentation missed retained simulation history");
    }
    ++next_;
  }
}

void Presentation::consume(CoopMatch &match, const std::function<void(std::uint32_t)> &present) {
  if (frames_.empty()) {
    return;
  }
  const auto current = match.checkpoint();
  try {
    while (!frames_.empty()) {
      const auto frame = std::move(frames_.front());
      frames_.pop_front();
      match.restore_checkpoint(*frame.state);
      present(frame.tick);
    }
  } catch (...) {
    match.restore_checkpoint(*current);
    throw;
  }
  match.restore_checkpoint(*current);
}
} // namespace phantomcrash::online
