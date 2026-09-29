#pragma once

#include "phantomcrash/canvas.hpp"
#include "phantomcrash/input.hpp"
#include "phantomcrash/rumble.hpp"

#include <SDL3/SDL.h>

#include <array>
#include <optional>
#include <vector>

namespace phantomcrash {

// Keyboard and SDL gamepads mapped onto Xbox buttons (after the Lodoss boot
// input). Presses are latched from events, so a tap shorter than a logic tick
// still registers once.
//
// Keyboard: Enter = START, Z/Space = A, X/Backspace = B, C = X, V = Y,
// Esc = BACK, arrows or WASD = D-pad (and the left stick), Q/E = left/right
// trigger, R/F = white/black, Left Shift / T = left/right stick click,
// J/L/I/K = the right stick.
// Gamepad: face buttons by position (south = A), shoulders = white/black.
// Mouse: its position over the window and left clicks, for the screens that
// take them (the town map).
// Ports (the VS mode's players): the keyboard is port 1 and the gamepads the
// next ones; with four gamepads they take the four ports, the keyboard
// sharing the first.
class DesktopInput final {
public:
  DesktopInput() = default;
  ~DesktopInput();
  DesktopInput(const DesktopInput &) = delete;
  DesktopInput &operator=(const DesktopInput &) = delete;

  void handle(const SDL_Event &event);
  // A tap of `buttons` on `port` for the next logic tick (scripted smoke
  // runs); a port pressed this way counts as connected from then on.
  void press(Buttons buttons, int port = 0);
  // `buttons` held down on `port` for the next logic tick (scripted smoke runs).
  void hold(Buttons buttons, int port = 0);
  // The mouse moved to `canvas` (and clicked there) for the next logic tick
  // (scripted smoke runs); it stays there.
  void point(std::array<float, 2> canvas, bool click) {
    scripted_pointer_ = canvas;
    scripted_moved_ = true;
    scripted_click_ = scripted_click_ || click;
  }
  // The frame for the next logic tick, the mouse placed on `viewport` (the
  // picture as drawn). Without focus nothing is held.
  InputFrame sample(bool focused, const Viewport &viewport);
  // Sets the gamepads' motors: each pad its own port's, and the first
  // port's too, since the player's robot takes every controller's buttons.
  // Sent when they change, and again while they last.
  void rumble(const std::array<RumbleMotors, controller_ports> &motors);
  // Gamepads plus the keyboard, which counts as one controller.
  [[nodiscard]] int controllers() const {
    return static_cast<int>(gamepads_.size()) + 1;
  }

private:
  using PortButtons = std::array<Buttons, controller_ports>;

  // The port gamepad `pad` (in connection order) is on.
  [[nodiscard]] int port_of(std::size_t pad) const;
  [[nodiscard]] PortButtons held_buttons() const;
  void fill_ports(bool focused, const PortButtons &held, InputFrame &frame);
  // The analog sticks, or the held directions as the left stick.
  void sticks(Buttons held, InputFrame &frame) const;

  std::vector<SDL_Gamepad *> gamepads_;
  struct PadRumble {
    RumbleMotors motors; // what the pad was last sent
    Uint64 sent_ms{};
  };
  std::vector<PadRumble> rumbling_;
  PortButtons latched_{};
  PortButtons injected_{};
  PortButtons injected_held_{};
  PortButtons previous_{};
  std::array<bool, controller_ports> scripted_ports_{};
  InputEdges edges_;
  std::optional<std::array<float, 2>> pointer_pixels_; // over the window
  bool pointer_moved_{};
  bool clicked_{};
  std::optional<std::array<float, 2>> scripted_pointer_;
  bool scripted_moved_{};
  bool scripted_click_{};
};

} // namespace phantomcrash
