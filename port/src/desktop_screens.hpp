#pragma once

#include "desktop_audio.hpp"
#include "desktop_screen.hpp"
#include "desktop_textures.hpp"
#include "phantomcrash/data_roots.hpp"
#include "phantomcrash/front_end.hpp"
#include "phantomcrash/glyph_font.hpp"
#include "phantomcrash/intro_sequence.hpp"
#include "phantomcrash/button_config.hpp"
#include "phantomcrash/option_menu.hpp"
#include "phantomcrash/screen_adjust.hpp"
#include "phantomcrash/settings.hpp"
#include "phantomcrash/sound_config.hpp"

#include <SDL3/SDL.h>

#include <map>
#include <memory>
#include <string>

namespace phantomcrash {

// Runs one screen at a time and switches to the one it names when done:
// "intro", "title", "main_menu", "option", "sound_config", "button_config",
// "screen_adjust", or a placeholder from data/ui/placeholders.json. Plays
// each screen's music and sound effects at the game's volumes, moves the
// picture by the screen offset, and keeps the game options the screens change.
class ScreenRunner final {
public:
  ScreenRunner(SDL_Renderer *renderer, const DataRoots &roots, const std::string &first,
               GameOptions options);

  void tick(const InputFrame &input, double tick_seconds);
  // Once per presented frame.
  void advance(double elapsed_seconds);
  void draw(const Viewport &viewport);
  void set_paused(bool paused);

  [[nodiscard]] const std::string &screen() const noexcept {
    return name_;
  }
  [[nodiscard]] const GameOptions &game_options() const noexcept {
    return options_;
  }

private:
  struct Placeholder {
    std::string label;
    std::string music;
    std::string return_to;
  };

  void show(const std::string &name);
  void apply_volumes();
  // The credit line for a music track, in `format`.
  [[nodiscard]] std::string now_playing(const std::string &track, const std::string &format) const;
  [[nodiscard]] std::unique_ptr<Screen> make(const std::string &name, const std::string &from);

  const DataRoots &roots_;
  TextureCache textures_;
  AudioMixer audio_;
  IntroScript intro_;
  TitleScript title_;
  MainMenuScript menu_;
  OptionScript option_;
  SoundConfigScript sound_config_;
  ButtonConfigScript button_config_;
  ScreenAdjustScript screen_adjust_;
  MessageBoxStyle message_box_;
  GlyphFont font_;
  std::vector<MusicCredit> credits_;
  std::vector<int> volume_millibels_;              // tables/music.json
  std::map<std::string, std::string> track_files_; // music name -> file
  GameOptions options_;
  std::map<std::string, Placeholder> placeholders_;
  Buttons placeholder_buttons_{};
  std::string placeholder_return_;
  std::unique_ptr<Screen> screen_;
  std::string name_;
  int intro_pass_{};
};

} // namespace phantomcrash
