#include "desktop_online_session.hpp"
#include "desktop_online_presentation.hpp"
#include <stdexcept>

namespace phantomcrash::online {

MatchSession::MatchSession(Room &room, CoopMatch &match, const std::uint64_t timeline,
                           const std::uint8_t players, MatchAssets *assets,
                           const std::uint32_t first_tick)
    : room_(room), match_(match), players_(players),
      simulation_(match, players, first_tick, assets),
      prediction_(simulation_, players, first_tick), inputs_(room, prediction_, timeline, players),
      states_(room, timeline, first_tick) {}

bool MatchSession::receive(const RoomMessage &message, const double now) {
  return inputs_.receive(message, now) || states_.receive(message);
}

void MatchSession::check_state(const double now) {
  states_.update(
      std::min(next_tick(), confirmed_tick()),
      [this](const auto tick) { return confirmed_digest(tick); }, now);
}

void MatchSession::reconcile() {
  prediction_.reconcile();
}

void MatchSession::collect_presentation() {
  if (!(players_ & (1U << room_.local_player()))) {
    return;
  }
  if (const auto presentation = match_.presentations[room_.local_player()].lock()) {
    presentation->collect(match_, prediction_);
  }
}

bool MatchSession::needs_input() const {
  return !frozen_ && !room_.recovering() && input_due();
}

bool MatchSession::input_due() const {
  return (players_ & (1U << room_.local_player())) &&
         (!captured_ || captured_->tick != prediction_.next_tick());
}

void MatchSession::capture(const RobotControl &control, const FireControl &fire,
                           std::optional<TargetView> view, const double now) {
  if (!needs_input()) {
    throw std::runtime_error("Online tick already captured or local player is not in this match");
  }
  PlayerInput input;
  input.tick = prediction_.next_tick();
  input.player = room_.local_player();
  input.control = control;
  input.fire = fire;
  input.view = std::move(view);
  inputs_.submit(input, now);
  captured_ = std::move(input);
}

bool MatchSession::advance() {
  if (frozen_ || input_due()) {
    return false;
  }
  return prediction_.advance();
}

std::uint32_t MatchSession::next_tick() const {
  return prediction_.next_tick();
}
std::uint32_t MatchSession::confirmed_tick() const {
  return prediction_.confirmed_tick();
}
std::uint64_t MatchSession::rollback_count() const {
  return prediction_.rollback_count();
}

std::optional<StateDigest> MatchSession::confirmed_digest(const std::uint32_t tick) {
  std::optional<StateDigest> result;
  prediction_.inspect_confirmed(tick, [&] { result = state_digest(match_); });
  return result;
}

void MatchSession::freeze() {
  frozen_ = true;
}

std::uint8_t MatchSession::confirmed_finished() {
  const auto tick = std::min(next_tick(), confirmed_tick());
  if (result_tick_ != tick) {
    if (!prediction_.inspect_confirmed(tick, [&] {
          finished_ = 0;
          for (int slot = 0; slot < max_players; ++slot) {
            if (match_.players[slot].actor >= 0 && match_.players[slot].finished) {
              finished_ |= 1U << slot;
            }
          }
        })) {
      throw std::runtime_error("Confirmed match result is outside retained history");
    }
    result_tick_ = tick;
  }
  return finished_;
}

std::uint32_t MatchSession::frozen_horizon() const {
  return next_tick() + (captured_ && captured_->tick == next_tick() ? 1U : 0U);
}

bool MatchSession::align(const std::uint32_t target, const double now) {
  if (!frozen_ || target < frozen_horizon()) {
    throw std::runtime_error("Invalid online transition alignment");
  }
  if (room_.recovering()) {
    return false;
  }
  prediction_.reconcile();
  if (next_tick() < target) {
    if (input_due()) {
      auto input = predict_input(captured_.value_or(PlayerInput{}), next_tick());
      input.player = room_.local_player();
      inputs_.submit(input, now);
      captured_ = input;
    }
    static_cast<void>(prediction_.advance());
  }
  collect_presentation();
  return next_tick() == target && confirmed_tick() >= target;
}
} // namespace phantomcrash::online
