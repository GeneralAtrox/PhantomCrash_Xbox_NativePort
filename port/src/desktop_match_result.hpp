#pragma once

#include "desktop_frame.hpp"
#include "desktop_screen.hpp"
#include "desktop_world_renderer.hpp"
#include "phantomcrash/match_result.hpp"
#include "desktop_replay.hpp"

namespace phantomcrash {

// A settled receipt over the last battle view. Animation never changes the save.
class MatchResultScreen final : public Screen {
public:
  MatchResultScreen(TextureCache &textures, const GlyphFont &font, const FrameStyle &frame,
                    const std::vector<MusicCredit> &credits, const json::JsonValue &data,
                    MatchReceipt receipt, QuestSave quest, WorldRenderer *world,
                    WorldFrame background, std::string next, std::shared_ptr<ReplayArchive> replay);
  void tick(const InputFrame &input, double seconds) override;
  void draw(const Viewport &viewport) override;
  bool done() const override {
    return done_;
  }
  std::string next() const override {
    return next_;
  }
  std::string music() const override {
    return watching_ ? watching_->music() : music_;
  }
  void set_music(const std::string &file) override {
    frame_.set_music(file);
  }
  void hear_music(const std::vector<float> &levels) override {
    frame_.hear_music(levels);
  }
  std::vector<SoundCue> take_sound_cues() override;

private:
  void text(DrawList &list, const std::string &value, float x, float y, std::uint32_t colour,
            float scale = 1.0F, bool right = false) const;
  TextureCache &textures_;
  const GlyphFont &font_;
  LocationFrame frame_;
  MatchReceipt receipt_;
  QuestSave quest_;
  WorldRenderer *world_;
  WorldFrame background_;
  std::string next_, music_;
  std::vector<Sprite> sprites_;
  Sprite prize_label_, rental_label_, retire_, exit_, replay_;
  Sprite exit_selected_, replay_selected_;
  std::shared_ptr<ReplayArchive> tape_;
  std::unique_ptr<ReplayScreen> watching_;
  json::JsonValue replay_data_;
  std::vector<SoundCue> pending_sounds_;
  int selected_{1};
  TickerStyle hint_;
  std::vector<int> hint_run_;
  std::array<float, 9> rows_{};
  float values_right_{};
  double seconds_{}, tally_seconds_{};
  bool done_{};
};

} // namespace phantomcrash
