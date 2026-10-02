#pragma once

#include "desktop_screens.hpp"

namespace phantomcrash {

// Owns independent menu flows. Each pane renders to its own native-resolution
// target because legacy screens clear the complete SDL render target.
class CoopScreen final : public Screen {
public:
  CoopScreen(SDL_Renderer *renderer, const DataRoots &roots, GameOptions options,
             ScreenRunner::SaveSlots imports, QuestSave current, std::shared_ptr<CoopState> state,
             const GlyphFont &font);
  ~CoopScreen() override;
  void tick(const InputFrame &input, double seconds) override;
  void advance(double seconds) override;
  void draw(const Viewport &viewport) override;
  void set_paused(bool paused) override;
  void set_presentation(const AudioOptions &audio, const PostProcessOptions &post) override;
  std::array<RumbleMotors, controller_ports> session_rumble() const override;
  // Each joined player's screen and save, for the scene dump.
  json::JsonValue scene_details() const override;
  // The first player's scene that is playing out, named for its player.
  std::optional<SceneProgress> scene_progress() const override;
  bool done() const override {
    return done_;
  }
  std::string next() const override {
    return "main_menu";
  }

private:
  struct Player {
    int port{-1};
    int choice{};
    bool ready{};
    bool save_error{};
    bool connected{true};
    std::unique_ptr<ScreenRunner> runner;
    SDL_Texture *target{};
    int width{}, height{};
  };
  void lobby_input(int slot, const InputFrame &input);
  void load_player(int slot);
  void ready_player(int slot);
  void persist(int slot);
  std::string choice_label(int slot) const;
  void text(const Viewport &viewport, const std::string &line, float x, float y, float size = 14,
            SDL_Color colour = {255, 255, 255, 255});
  InputFrame player_input(const InputFrame &input, int slot) const;
  void draw_presence(int slot, const Viewport &viewport);
  SDL_Renderer *renderer_;
  TextureCache textures_;
  const GlyphFont &font_;
  TownMapScript map_;
  const DataRoots &roots_;
  GameOptions options_;
  ScreenRunner::SaveSlots imports_;
  QuestSave current_;
  std::shared_ptr<CoopState> state_;
  std::array<Player, 4> players_{};
  AudioOptions audio_;
  PostProcessOptions post_;
  Viewport viewport_;
  bool playing_{};
  bool done_{};
};

} // namespace phantomcrash
