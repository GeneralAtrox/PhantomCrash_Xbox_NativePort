#pragma once

#include "desktop_online_match.hpp"
#include "phantomcrash/online_input_channel.hpp"
#include "phantomcrash/online_state_channel.hpp"

namespace phantomcrash::online {

// A running, agreed match timeline. Room establishment and initial-state
// agreement happen before construction. Menus continue to use the same Room.
class MatchSession final {
public:
  MatchSession(Room &room, CoopMatch &match, std::uint64_t timeline, std::uint8_t players,
               MatchAssets *assets = nullptr, std::uint32_t first_tick = 0);
  bool receive(const RoomMessage &message, double now);
  void reconcile();
  void check_state(double now);
  void collect_presentation();
  [[nodiscard]] InputFrame confirmed_frame(std::uint32_t tick) const {
    return prediction_.confirmed_frame(tick);
  }
  [[nodiscard]] std::uint32_t checked_tick() const {
    return states_.checked_tick();
  }
  // Captures exactly once per network tick. The caller keeps unsampled input
  // while waiting; it must not overwrite an input that has already been sent.
  void capture(const RobotControl &control, const FireControl &fire, std::optional<TargetView> view,
               double now);
  [[nodiscard]] bool needs_input() const;
  [[nodiscard]] const std::optional<PlayerInput> &local_input() const {
    return captured_;
  }
  bool advance();
  [[nodiscard]] std::uint32_t next_tick() const;
  [[nodiscard]] std::uint32_t confirmed_tick() const;
  [[nodiscard]] std::uint64_t rollback_count() const;
  [[nodiscard]] std::optional<StateDigest> confirmed_digest(std::uint32_t tick);
  [[nodiscard]] std::uint8_t confirmed_finished();
  // Membership transitions first freeze every peer, then agree an alignment
  // tick covering all controls already captured by any peer.
  void freeze();
  [[nodiscard]] std::uint32_t frozen_horizon() const;
  bool align(std::uint32_t target, double now);

private:
  Room &room_;
  CoopMatch &match_;
  std::uint8_t players_{};
  MatchSimulation simulation_;
  Prediction prediction_;
  InputChannel inputs_;
  StateChannel states_;
  std::optional<PlayerInput> captured_;
  bool frozen_{};
  std::optional<std::uint32_t> result_tick_;
  std::uint8_t finished_{};
  [[nodiscard]] bool input_due() const;
};
} // namespace phantomcrash::online
