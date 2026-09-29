#pragma once

#include "phantomcrash/canvas.hpp"
#include "phantomcrash/input.hpp"
#include "phantomcrash/rumble.hpp"
#include "phantomcrash/rumbling_entry.hpp"
#include "phantomcrash/settings.hpp"
#include "phantomcrash/world_sound.hpp"

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
  [[nodiscard]] virtual bool mouse_aim() const {
    return false;
  }

  // The music track this screen wants (empty: silence; "keep": whatever
  // plays on), and its volume.
  [[nodiscard]] virtual std::string music() const {
    return {};
  }
  [[nodiscard]] virtual float music_volume() const {
    return 1.0F;
  }
  // The music file playing now, or empty for silence: told as the screen
  // opens and whenever it changes, for the screens that show its credit.
  virtual void set_music(const std::string & /*file*/) {}
  // The CURRENT MUSIC meter's levels for the music heard now, each tick
  // before tick(), for the screens that show the meter.
  virtual void hear_music(const std::vector<float> & /*levels*/) {}
  // Sound effects started since the last call.
  [[nodiscard]] virtual std::vector<std::string> take_sounds() {
    return {};
  }
  // Placed sounds and keyed loops since the last call (the battle's).
  // The controllers whose robots took damage since the last take (rumble.hpp).
  [[nodiscard]] virtual std::vector<DamageRumble> take_rumble() {
    return {};
  }
  [[nodiscard]] virtual std::vector<SoundCue> take_sound_cues() {
    return {};
  }
  // Effects the screen may play, loaded as it opens.
  [[nodiscard]] virtual std::vector<std::string> sound_preloads() const {
    return {};
  }
  // Sound effects that stop as the screen closes, as the original's
  // destroy steps stop them.
  [[nodiscard]] virtual std::vector<std::string> held_sounds() const {
    return {};
  }
  [[nodiscard]] virtual bool done() const = 0;
  // The screen to show next, once done.
  [[nodiscard]] virtual std::string next() const = 0;
  // The game options as this screen has left them, for screens that change them.
  [[nodiscard]] virtual const GameOptions *game_options() const {
    return nullptr;
  }
  // The quest save as this screen has left it, for screens that change it.
  [[nodiscard]] virtual const QuestSave *quest_save() const {
    return nullptr;
  }
  // The parts-stock entry NAME ENTRY names when it comes next (a dealer's
  // new CHIP, 0x3ad84), or -1.
  [[nodiscard]] virtual int naming_stock() const {
    return -1;
  }
  // The hangar whose Scoobee NAME ENTRY names when it comes next
  // (ASSEMBLE's NAME), or -1.
  [[nodiscard]] virtual int naming_hangar() const {
    return -1;
  }
  // The hangar whose Scoobee the hangar's KEEP chose for KEEP SV, or -1.
  [[nodiscard]] virtual int kept_hangar() const {
    return -1;
  }
  // The battle RUMBLING enters when it comes next, or none.
  [[nodiscard]] virtual const RumblingEntry *rumbling_entry() const {
    return nullptr;
  }
  // The live RUMBLING's opponents as they now stand (rumbling_world.hpp).
  virtual void set_live_rosters(const std::array<AreaRoster, roster_areas> & /*rosters*/) {}
};

} // namespace phantomcrash
