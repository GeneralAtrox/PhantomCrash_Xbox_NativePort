#pragma once

#include <algorithm>

namespace phantomcrash {

// Game logic advances in whole ticks of a fixed length, whatever the
// presentation rate: add() the real time since the last frame, run the
// returned number of ticks, then draw with alpha() between the last two.
class FixedStep final {
public:
  explicit FixedStep(double tick_seconds, int max_ticks_per_frame = 8)
      : tick_(tick_seconds), max_ticks_(max_ticks_per_frame) {}

  // Returns the ticks due. After a stall longer than `max_ticks_per_frame`
  // ticks the backlog is dropped rather than replayed in a burst.
  int add(double elapsed_seconds) {
    accumulator_ += std::max(0.0, elapsed_seconds);
    int ticks = static_cast<int>(accumulator_ / tick_);
    accumulator_ -= ticks * tick_;
    if (ticks > max_ticks_) {
      ticks = max_ticks_;
      accumulator_ = 0.0;
    }
    return ticks;
  }

  // Fraction of the next tick already elapsed, in [0, 1).
  [[nodiscard]] double alpha() const {
    return std::clamp(accumulator_ / tick_, 0.0, 1.0);
  }

  void reset() {
    accumulator_ = 0.0;
  }

  [[nodiscard]] double tick_seconds() const {
    return tick_;
  }

private:
  double tick_;
  int max_ticks_;
  double accumulator_{};
};

} // namespace phantomcrash
