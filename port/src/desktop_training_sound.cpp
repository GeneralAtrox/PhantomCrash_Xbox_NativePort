// The training battle's sounds: the robots' own, their weapons', the
// doors' and the pickups', each placed where it happens and heard from the
// camera (world_sound.hpp). The split screen hears them whole, as the
// original does (0x09df75).

#include "desktop_training.hpp"

#include <SDL3/SDL.h>

namespace phantomcrash {

namespace {

constexpr const char *battle_sounds_document = "audio/battle_sounds.json";

} // namespace

void TrainingScreen::queue_dialogue(std::vector<std::string> sounds) {
  for (auto &sound : sounds) {
    SoundCue cue{SoundCue::Action::play, std::move(sound)};
    cue.bus = AudioBus::dialogue;
    unplaced_.push_back(std::move(cue));
  }
}

void TrainingScreen::load_sounds(const DataRoots &roots) {
  try {
    battle_sounds_ =
        std::make_unique<BattleSounds>(parse_battle_sounds(roots.document(battle_sounds_document)));
  } catch (const std::exception &error) {
    SDL_Log("%s: %s", battle_sounds_document, error.what());
  }
}

void TrainingScreen::gather_sounds(const float seconds) {
  std::vector<WorldSound> sounds;
  if (StageDoors *doors = battle_->doors()) {
    sounds = doors->take_sounds();
  }
  collect_supply(*battle_, script_.battle.supply.look, money_, sounds);
  if (StageObjects *objects = battle_->objects()) {
    for (auto &[name, at] : objects->take_sounds()) {
      sounds.push_back({std::move(name), at});
    }
  }
  for (WorldSound &sound : sounds) {
    SoundCue cue{SoundCue::Action::play, std::move(sound.name)};
    cue.bus = sound.bus;
    if (sound.at) {
      placed_.push_back({std::move(cue), *sound.at});
    } else {
      unplaced_.push_back(std::move(cue));
    }
  }
  if (battle_sounds_) {
    battle_sounds_->observe(*battle_, seconds);
    std::ranges::move(battle_sounds_->take(), std::back_inserter(placed_));
  }
}

std::vector<std::string> TrainingScreen::sound_preloads() const {
  std::vector<std::string> files;
  if (battle_sounds_) {
    files = sound_files(battle_sounds_->table());
  }
  files.push_back(script_.battle.settings.robot_wrecks.break_sound);
  const SceneTuning &scenes = script_.battle.scenes;
  for (const std::string *name :
       {&scenes.doors.move_sound, &scenes.doors.open_sound, &scenes.ready_sound, &scenes.go_sound,
        &script_.battle.supply.look.take_sound}) {
    files.push_back(*name);
  }
  return files;
}

std::vector<SoundCue> TrainingScreen::take_sound_cues() {
  const SoundFalloff falloff = battle_sounds_ ? battle_sounds_->table().falloff : SoundFalloff{};
  SoundListener listener{camera_.eye()};
  const Vec3 view = camera_.target() - camera_.eye();
  if (length(view) > 1e-4F) {
    listener.forward = normalize(view);
  }
  auto cues = place_cues(falloff, screens_ > 1 ? nullptr : &listener, std::exchange(placed_, {}));
  std::ranges::move(std::exchange(unplaced_, {}), std::back_inserter(cues));
  if (replay_) {
    replay_->hear(cues);
  }
  return cues;
}

} // namespace phantomcrash
