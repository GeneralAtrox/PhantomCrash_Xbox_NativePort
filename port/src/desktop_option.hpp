#pragma once

#include "desktop_screen.hpp"
#include "desktop_textures.hpp"
#include "phantomcrash/glyph_font.hpp"
#include "phantomcrash/option_menu.hpp"

#include <string>
#include <vector>

namespace phantomcrash {

// The OPTION screen (OptionMenu): its list, the scrolling help and music
// lines, and the message boxes, drawn in the original's layer order.
class OptionScreen final : public Screen {
public:
  // `now_playing` is the credit line for the screen's music (may be empty).
  OptionScreen(TextureCache &textures, const GlyphFont &font, OptionScript script,
               MessageBoxStyle box, GameOptions options, const std::string &now_playing,
               const std::string &returning_from);

  void tick(const InputFrame &input, double tick_seconds) override;
  void draw(const Viewport &viewport) override;
  [[nodiscard]] std::string music() const override {
    return menu_.music();
  }
  [[nodiscard]] std::vector<std::string> take_sounds() override {
    return menu_.take_sounds();
  }
  [[nodiscard]] bool done() const override {
    return menu_.done();
  }
  [[nodiscard]] std::string next() const override {
    return menu_.next();
  }
  [[nodiscard]] const GameOptions *game_options() const override {
    return &menu_.options();
  }

private:
  TextureCache &textures_;
  const GlyphFont &font_;
  OptionMenu menu_;
  std::vector<std::vector<int>> help_runs_;
  std::vector<int> now_playing_run_;
};

} // namespace phantomcrash
