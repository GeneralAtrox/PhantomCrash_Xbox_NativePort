#pragma once

#include "desktop_battle_chatter.hpp"
#include "desktop_battle_hud.hpp"
#include "desktop_battle_scene.hpp"
#include "desktop_battle_view.hpp"
#include "desktop_cockpit.hpp"
#include "desktop_effects.hpp"
#include "desktop_shots.hpp"
#include "desktop_screen.hpp"
#include "desktop_supply.hpp"
#include "desktop_textures.hpp"
#include "desktop_world_renderer.hpp"
#include "phantomcrash/battle.hpp"
#include "phantomcrash/battle_sounds.hpp"
#include "phantomcrash/battle_camera.hpp"
#include "phantomcrash/chase_camera.hpp"
#include "phantomcrash/data_roots.hpp"
#include "phantomcrash/event_player.hpp"
#include "phantomcrash/glyph_font.hpp"
#include "phantomcrash/robot_library.hpp"
#include "phantomcrash/rumbling_world.hpp"
#include "phantomcrash/split_screen.hpp"
#include "phantomcrash/training_ground.hpp"
#include "phantomcrash/tutorial.hpp"

#include <cstdint>
#include <memory>
#include <string>
#include <vector>

namespace phantomcrash {

// Everything the tutorial battle reads that outlives the screen.
struct TrainingContext {
  const DataRoots &roots;
  WorldRenderer *world; // null or unavailable: nothing 3D is drawn
  RobotLibrary *robots; // null or unavailable: no battle
  const TrainingScript &script;
  const StageDefinition *stage;
  const TalkTables &talk;
  EventTuning tuning;
  std::string player_name;
  bool standard_controls{true}; // BUTTON CONFIG's CONTROL TYPE is Standard
  int day{};                    // the quest's day and money, for the HUD
  std::int64_t money{};
  // The live RUMBLING's battle the player joins (rumbling_world.hpp), its
  // opponents already out, and the world they come from; null for a
  // battle of the screen's own.
  std::shared_ptr<Battle> live_battle{};
  const RumblingWorld *live_world{};
  int live_area{-1};
};

// The tutorial battle (scene 42) on the training ground, opened by one of
// Pepper's lessons: the player's robot against the drones walking the
// stage's enemy path, with Pepper's messages leading through the lesson's
// steps (robot states 0x18-0x2e) and back to her when the course ends.
// FreePlay has no course: the robot walks out of a gate and the battle
// runs until it walks back into one, or until the exit button.
class TrainingScreen final : public Screen {
public:
  [[nodiscard]] bool mouse_aim() const override {
    return true;
  }
  TrainingScreen(TextureCache &textures, const GlyphFont &font, const TrainingContext &context,
                 const TrainingLesson &lesson);

  void tick(const InputFrame &input, double tick_seconds) override;
  void draw(const Viewport &viewport) override;
  // An entrance's jingle plays in place of the battle's music until it ends.
  [[nodiscard]] std::string music() const override {
    return cue_music_.empty() ? script_.music : cue_music_;
  }
  void set_music(const std::string &file) override {
    if (file.empty()) {
      cue_music_.clear(); // the jingle is over (or would not play)
    }
  }
  [[nodiscard]] std::vector<std::string> take_sounds() override {
    return std::exchange(sounds_, {});
  }
  // The battle's sounds, placed as the camera hears them
  // (desktop_training_sound.cpp).
  [[nodiscard]] std::vector<SoundCue> take_sound_cues() override;
  // The player's robot is port 1's (the first robot, Combatant::piloted).
  [[nodiscard]] std::vector<DamageRumble> take_rumble() override {
    return std::exchange(rumble_, {});
  }
  [[nodiscard]] std::vector<std::string> sound_preloads() const override;
  [[nodiscard]] bool done() const override {
    return done_;
  }
  [[nodiscard]] std::string next() const override {
    return script_.return_to;
  }

private:
  void build_battle(const TrainingContext &context);
  // The live RUMBLING (desktop_training_live.cpp): the player's robot
  // takes robot 0's place, and each opponent brought in since is drawn as
  // itself.
  void seat_player(const BattleSettings &settings, float hp);
  void add_live_views();
  void follow_live();
  void load_messages(const DataRoots &roots, bool standard_controls);
  void update_message(const InputFrame &input, double tick_seconds);
  void tick_battle(const InputFrame &input, float seconds);
  void load_sounds(const DataRoots &roots);
  // The robots', doors' and pickups' sounds of the tick just run.
  void gather_sounds(float seconds);
  // The view buttons, then the chase camera and the view on top of it
  // (desktop_training_camera.cpp).
  void tick_camera(const InputFrame &input, float seconds);
  [[nodiscard]] CameraSubject camera_subject() const;
  // Whether the camera sits in the player's robot, and whether (and how
  // far slid out of view) the cockpit is up.
  [[nodiscard]] bool in_cockpit() const;
  [[nodiscard]] bool cockpit_up() const;
  [[nodiscard]] CockpitInput cockpit_input() const;
  [[nodiscard]] bool in_scene() const {
    return scene_ && scene_->scene().scripted();
  }
  [[nodiscard]] BattleHudInfo hud_info() const;
  // Split screen (desktop_training_split.cpp): the further screens'
  // cameras and HUDs, their ticks, and one screen's world and HUD.
  void build_split(const DataRoots &roots);
  void tick_split(float seconds);
  void draw_screen(const Viewport &screen, std::size_t index);
  void draw_message(const Viewport &viewport);
  void draw_notice(const std::vector<int> &run, const Viewport &viewport);

  TextureCache &textures_;
  const GlyphFont &font_;
  WorldRenderer *world_;
  const TrainingScript &script_;
  const TalkTables &talk_;
  EventTuning tuning_;
  StageLighting lighting_;
  WorldItem stage_;
  int mode_{};
  bool free_play_{};
  LessonMessages messages_;
  Tutorial tutorial_;
  ChaseCamera camera_;
  CameraViews views_;
  std::shared_ptr<Battle> battle_; // the live RUMBLING's is shared with its world
  const RumblingWorld *live_world_{};
  int live_area_{-1};
  int live_changes_{};
  std::vector<int> live_entrants_; // drawn as, by battle robot
  std::unique_ptr<BattleView> view_;
  std::unique_ptr<BattleHud> hud_;
  // Screens after the first, each following its own robot; empty with one
  // screen.
  struct SplitView {
    int robot{};
    ChaseCamera camera;
    std::unique_ptr<BattleHud> hud;
    bool placed{}; // the camera has been put behind its robot
  };
  int screens_{1};
  std::vector<SplitView> splits_;
  std::unique_ptr<DesktopCockpit> cockpit_; // null without its model
  std::unique_ptr<ChatterWindow> chatter_;  // null without its lines
  // FreePlay's entrance and exit; null in the lessons, which start where
  // the script puts the robot.
  std::unique_ptr<BattleScenePresenter> scene_;
  int door_model_{-1};                      // the gate doors' leaves
  std::unique_ptr<DesktopSupply> supply_;   // FreePlay's; null without its model
  std::unique_ptr<DesktopEffects> effects_; // null without the effect bank
  std::unique_ptr<DesktopShots> shots_;     // with the effects, whose bank it draws
  std::string cue_music_;
  // The message playing now, its script kept for the player.
  std::unique_ptr<StoryEvent> message_event_;
  std::unique_ptr<EventPlayer> message_;
  std::vector<std::string> sounds_; // heard whole: the menus', the chatter's
  std::unique_ptr<BattleSounds> battle_sounds_;
  std::vector<PlacedCue> placed_; // heard where they are
  std::vector<DamageRumble> rumble_;
  std::vector<int> player_name_run_;
  std::vector<int> missing_run_;
  bool snap_camera_{true};
  bool jump_armed_{true}; // the button that closed a message must lift first
  bool done_{};
  double clock_{}; // seconds of battle: the HUD's mission time and pulses
  int day_{};
  std::int64_t money_{};
};

} // namespace phantomcrash
