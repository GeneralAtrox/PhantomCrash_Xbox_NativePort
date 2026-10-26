#pragma once

// A S.L.A.I. talk scene on the desktop: the scene's quads, laid out in the
// PS2 screen's 512x448 pixels, drawn over the 640x480 canvas (town scenes
// over the screen they interrupted), with its music and sounds.

#include "desktop_screen.hpp"
#include "desktop_textures.hpp"
#include "phantomcrash/slai_talk_scene.hpp"

#include <cstdint>
#include <map>
#include <memory>
#include <string>
#include <vector>

namespace phantomcrash {

class SlaiTalkScreen final : public Screen {
public:
  // `behind` is drawn under a town scene and is not ticked; null for none.
  SlaiTalkScreen(TextureCache &textures, std::unique_ptr<SlaiTalkScene> scene, std::string next,
                 std::unique_ptr<Screen> behind);

  void tick(const InputFrame &input, double tick_seconds) override;
  void draw(const Viewport &viewport) override;
  [[nodiscard]] std::string music() const override;
  [[nodiscard]] std::vector<std::string> take_sounds() override;
  // The scene's ambient loops: started, faded and stopped by key.
  [[nodiscard]] std::vector<SoundCue> take_sound_cues() override;
  [[nodiscard]] bool done() const override {
    return scene_->done();
  }
  [[nodiscard]] std::string next() const override {
    return next_;
  }
  [[nodiscard]] std::optional<SceneProgress> scene_progress() const override;

private:
  TextureCache &textures_;
  std::unique_ptr<SlaiTalkScene> scene_;
  std::string next_;
  std::unique_ptr<Screen> behind_;
  double pending_frames_{};
  std::map<int, float> loop_gains_; // by SlaiTalkLoop::id, as last sent
};

} // namespace phantomcrash
