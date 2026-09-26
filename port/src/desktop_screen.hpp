#pragma once

#include "phantomcrash/canvas.hpp"
#include "phantomcrash/input.hpp"
#include "phantomcrash/settings.hpp"

#include <string>
#include <vector>

namespace phantomcrash {

// One full-screen part of the game (intro, front end, placeholder), run by
// ScreenRunner. Logic runs on the fixed tick; advance() and draw() run once
// per presented frame.
class Screen {
public:
  virtual ~Screen() = default;

  virtual void tick(const InputFrame &input, double tick_seconds) = 0;
  virtual void advance(double /*elapsed_seconds*/) {}
  virtual void draw(const Viewport &viewport) = 0;
  virtual void set_paused(bool /*paused*/) {}

  // The music track this screen wants (empty: silence).
  [[nodiscard]] virtual std::string music() const {
    return {};
  }
  // Sound effects started since the last call.
  [[nodiscard]] virtual std::vector<std::string> take_sounds() {
    return {};
  }
  [[nodiscard]] virtual bool done() const = 0;
  // The screen to show next, once done.
  [[nodiscard]] virtual std::string next() const = 0;
  // The game options as this screen has left them, for screens that change them.
  [[nodiscard]] virtual const GameOptions *game_options() const {
    return nullptr;
  }
};

} // namespace phantomcrash
