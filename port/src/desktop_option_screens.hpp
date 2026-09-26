#pragma once

#include "desktop_screen.hpp"
#include "desktop_textures.hpp"
#include "phantomcrash/button_config.hpp"
#include "phantomcrash/glyph_font.hpp"
#include "phantomcrash/screen_adjust.hpp"
#include "phantomcrash/sound_config.hpp"

#include <string>
#include <vector>

namespace phantomcrash {

// A Screen over one of the option sub-screens' menus, which share their
// interface.
template <typename Menu> class MenuScreen : public Screen {
public:
  template <typename... Arguments>
  explicit MenuScreen(TextureCache &textures, Arguments &&...arguments)
      : textures_(textures), menu_(std::forward<Arguments>(arguments)...) {}

  void tick(const InputFrame &input, const double tick_seconds) override {
    menu_.update(tick_seconds, input);
  }
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

protected:
  TextureCache &textures_;
  Menu menu_;
};

// SOUND CONFIG: the items, their values as digits and bars, and the help and
// music lines.
class SoundConfigScreen final : public MenuScreen<SoundConfigMenu> {
public:
  SoundConfigScreen(TextureCache &textures, const GlyphFont &font, SoundConfigScript script,
                    GameOptions options, const std::string &now_playing);
  void draw(const Viewport &viewport) override;

private:
  const GlyphFont &font_;
  std::vector<std::vector<int>> help_runs_;
  std::vector<int> now_playing_run_;
};

// BUTTON CONFIG: the rows' bars, labels and values, the assigned buttons'
// icons, the selected row's arrows or controller marker, and the lines.
class ButtonConfigScreen final : public MenuScreen<ButtonConfigMenu> {
public:
  ButtonConfigScreen(TextureCache &textures, const GlyphFont &font, ButtonConfigScript script,
                     GameOptions options, const std::string &now_playing);
  void draw(const Viewport &viewport) override;

private:
  const GlyphFont &font_;
  std::vector<std::vector<int>> help_runs_;
  std::vector<int> now_playing_run_;
};

// SCREEN ADJUST: the picture, moved by the runner with the offset, and the
// labels held in place against it.
class ScreenAdjustScreen final : public MenuScreen<ScreenAdjustMenu> {
public:
  ScreenAdjustScreen(TextureCache &textures, const GlyphFont &font, ScreenAdjustScript script,
                     MessageBoxStyle box, GameOptions options);
  void draw(const Viewport &viewport) override;

private:
  const GlyphFont &font_;
};

} // namespace phantomcrash
