#pragma once

#include "phantomcrash/input.hpp"

#include <SDL3/SDL.h>

#include <vector>

namespace phantomcrash {

// Keyboard and SDL gamepads mapped onto Xbox buttons (after the Lodoss boot
// input). Presses are latched from events, so a tap shorter than a logic tick
// still registers once.
//
// Keyboard: Enter = START, Z/Space = A, X/Backspace = B, C = X, V = Y,
// Esc = BACK, arrows = D-pad, Q/E = left/right trigger.
// Gamepad: face buttons by position (south = A), shoulders = white/black.
class DesktopInput final {
public:
  DesktopInput() = default;
  ~DesktopInput();
  DesktopInput(const DesktopInput &) = delete;
  DesktopInput &operator=(const DesktopInput &) = delete;

  void handle(const SDL_Event &event);
  // A tap of `buttons` for the next logic tick (scripted smoke runs).
  void press(Buttons buttons) {
    injected_ |= buttons;
  }
  // The frame for the next logic tick. Without focus nothing is held.
  InputFrame sample(bool focused);
  // Gamepads plus the keyboard, which counts as one controller.
  [[nodiscard]] int controllers() const {
    return static_cast<int>(gamepads_.size()) + 1;
  }

private:
  [[nodiscard]] Buttons held_buttons() const;

  std::vector<SDL_Gamepad *> gamepads_;
  Buttons latched_{};
  Buttons injected_{};
  InputEdges edges_;
};

} // namespace phantomcrash
