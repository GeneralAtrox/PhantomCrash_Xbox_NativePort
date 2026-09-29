#pragma once

#include "desktop_screen.hpp"
#include "desktop_textures.hpp"
#include "movie_player.hpp"
#include "phantomcrash/canvas.hpp"
#include "phantomcrash/data_roots.hpp"
#include "phantomcrash/intro_sequence.hpp"

#include <SDL3/SDL.h>

#include <memory>
#include <optional>
#include <string>
#include <vector>

namespace phantomcrash {

// Plays data/boot/intro.json: movies through MoviePlayer, logo screens as
// textured layers, with the fades from IntroSequence. The next movie is
// opened while the current step runs, so steps follow without black gaps.
class IntroScreen final : public Screen {
public:
  IntroScreen(TextureCache &textures, const DataRoots &roots, IntroScript script, bool first_boot,
              int pass);
  ~IntroScreen() override = default;
  IntroScreen(const IntroScreen &) = delete;
  IntroScreen &operator=(const IntroScreen &) = delete;

  // Logic tick: input, step timing and transitions.
  void tick(const InputFrame &input, double tick_seconds) override;
  // Once per presented frame: movie decoding and sound, in real time.
  void advance(double elapsed_seconds) override;
  void set_paused(bool paused) override;
  void draw(const Viewport &viewport) override;

  [[nodiscard]] bool done() const override {
    return sequence_.done();
  }
  [[nodiscard]] std::string next() const override {
    return sequence_.then();
  }
  [[nodiscard]] std::vector<std::string> take_sounds() override {
    return sequence_.take_sounds();
  }

private:
  void enter_step();
  void open_movie(MoviePlayer &player, const std::string &file);
  void draw_image(const IntroStep &step, const Viewport &viewport);

  TextureCache &textures_;
  SDL_Renderer *renderer_{};
  const DataRoots &roots_;
  IntroSequence sequence_;
  std::unique_ptr<MoviePlayer> current_;
  std::unique_ptr<MoviePlayer> preloaded_;
  std::optional<std::string> preloaded_file_;
  int seen_generation_{-1};
  bool movie_failed_{};
};

} // namespace phantomcrash
