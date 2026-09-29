#pragma once

#include "phantomcrash/battle.hpp"
#include "phantomcrash/json.hpp"
#include "phantomcrash/world_sound.hpp"

#include <cstdint>
#include <string>
#include <vector>

namespace phantomcrash {

// One battle sound as the two banks have it: the players' robots play from
// battle_player (bank 3), the computer's from battle (bank 1) or
// battle_old (bank 2), the robot's +8 telling them apart. Either may be
// empty: that side plays nothing.
struct SoundPair {
  std::string player;
  std::string computer;

  [[nodiscard]] const std::string &pick(bool piloted) const noexcept {
    return piloted ? player : computer;
  }
};

// A weapon's sound, chosen by the first rule it matches: its record's
// fire sound handler (+0x21, table 0x2537c0), its mount, its model (+8),
// its HUD kind (+0x1c) and its flight (+0x1d). A rule's -1 or empty list
// matches any.
struct WeaponSoundRule {
  int style{-1};
  int mount{-1}; // 0 an arm, 1 a shoulder
  std::vector<int> models;
  int hud_kind{-1};
  int flight{-1};
  SoundPair sound;

  [[nodiscard]] bool matches(const WeaponTuning &weapon, int slot) const;
};

// data/audio/battle_sounds.json. The robots' own sounds (0x093637 and the
// functions it calls, docs/BATTLE_SOUNDS.md), the weapons' and the
// distance they carry.
struct BattleSoundTable {
  SoundFalloff falloff;
  SoundPair footstep;                      // 0x093cf0, from the walk's steps
  SoundPair landing;                       // 0x093df2
  SoundPair jump;                          // 0x08ea5b, 0x08d7fa
  SoundPair boost_start;                   // 0x0945c1: a dash starts ...
  SoundPair boost;                         // ... and this loops, following it, until it ends
  SoundPair dodge;                         // 0x094a8d, motions 13 and 14
  SoundPair air_dodge;                     // motions 15 and 16
  SoundPair shot_down;                     // 0x07675e
  SoundPair low_armour;                    // 0x093637: repeats while the armour is low
  float footstep_seconds{0.5F};            // between steps at the walk's full speed (port value)
  float low_armour_share{0.25F};           // below this share of the armour (0x199fd4)
  float low_armour_seconds{32.0F / 30.0F}; // 0x20 battle ticks between alarms
  std::vector<WeaponSoundRule> fire;       // 0x099c02
  std::vector<WeaponSoundRule> impact;     // 0x098eda, 0x047ed9
  SoundPair fire_fallback;                 // a weapon without a record
};

[[nodiscard]] BattleSoundTable parse_battle_sounds(const json::JsonValue &value);

// Every file the table names, once each.
[[nodiscard]] std::vector<std::string> sound_files(const BattleSoundTable &table);

// The first rule's sound for a weapon on `slot`, or nullptr.
[[nodiscard]] const SoundPair *weapon_sound(const std::vector<WeaponSoundRule> &rules,
                                            const WeaponTuning &weapon, int slot);

// A sound for the runner, with where it is in the world.
struct PlacedCue {
  SoundCue cue;
  Vec3 at;
};

// Places each cue as heard by `listener`, or whole without one (the
// split screen's).
[[nodiscard]] std::vector<SoundCue>
place_cues(const SoundFalloff &falloff, const SoundListener *listener, std::vector<PlacedCue> cues);

// Watches a battle tick by tick and says what it sounds like: every
// robot's steps, jumps, landings, dashes and dodges, its weapons and their
// hits, its end, and a player's low-armour alarm.
class BattleSounds final {
public:
  explicit BattleSounds(BattleSoundTable table) : table_(std::move(table)) {}

  // After each Battle::tick.
  void observe(const Battle &battle, float seconds);
  [[nodiscard]] std::vector<PlacedCue> take() {
    return std::exchange(cues_, {});
  }
  [[nodiscard]] const BattleSoundTable &table() const noexcept {
    return table_;
  }

private:
  struct Watch {
    MoveMode mode{MoveMode::stand};
    bool grounded{true};
    bool alive{true};
    bool boosting{};
    float step_left{};
    float alarm_left{};
  };

  void play(const std::string &name, Vec3 at);
  void observe_robot(std::size_t index, const Combatant &robot, float seconds);
  void observe_boost(std::size_t index, const Combatant &robot, Watch &watch);
  void observe_events(const Battle &battle);
  [[nodiscard]] static std::uint64_t boost_key(std::size_t index);

  BattleSoundTable table_;
  std::vector<Watch> watch_;
  std::vector<PlacedCue> cues_;
};

} // namespace phantomcrash
