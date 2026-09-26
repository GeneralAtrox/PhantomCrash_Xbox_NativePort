#pragma once

#include "phantomcrash/data_roots.hpp"
#include "phantomcrash/json.hpp"

#include <SDL3/SDL.h>

#include <array>
#include <cstdint>
#include <map>
#include <string>
#include <vector>

namespace phantomcrash {

// Music and sound effects on SDL audio streams, which SDL mixes on one
// device. Music streams from its WAV file and loops whole (the original's
// tracks have no loop points); effects are loaded once and played on a
// small pool of streams so they can overlap. Names come from
// data/audio/music.json and sfx.json. Missing files are logged, not fatal.
class AudioMixer final {
public:
  AudioMixer(const DataRoots &roots, const json::JsonValue &music, const json::JsonValue &effects);
  ~AudioMixer();
  AudioMixer(const AudioMixer &) = delete;
  AudioMixer &operator=(const AudioMixer &) = delete;

  // Starts a track, or stops the music for an empty name. The same track
  // carries on without a restart.
  void play_music(const std::string &track);
  // A named effect, or an effect file when the name has a '/' (SE TEST).
  void play_sound(const std::string &name);
  // The game's BGM and SE volumes as gains; they apply to what is playing.
  void set_volumes(float music, float effects);
  // Keeps the music queue topped up; once per frame.
  void update();
  void set_paused(bool paused);

  [[nodiscard]] const std::string &music() const noexcept {
    return track_;
  }

private:
  struct Track {
    std::string file;
    bool loop{true};
    float volume{1.0F};
  };
  struct Effect {
    SDL_AudioSpec spec{};
    std::vector<std::uint8_t> samples;
    float volume{1.0F};
    bool loaded{};
  };
  static constexpr std::size_t voice_count = 8;

  void stop_music();
  bool open_music(const Track &track);
  Effect *effect(const std::string &name);

  const DataRoots &roots_;
  SDL_AudioDeviceID device_{};
  std::map<std::string, Track> tracks_;
  std::map<std::string, Effect> effects_;
  std::map<std::string, std::pair<std::string, float>> effect_files_;
  std::string track_;
  SDL_AudioStream *music_{};
  SDL_IOStream *music_file_{};
  bool music_loop_{};
  float music_track_volume_{1.0F};
  float music_gain_{1.0F};
  float effect_gain_{1.0F};
  Sint64 data_start_{};
  Sint64 data_end_{};
  int music_bytes_per_second_{};
  std::array<SDL_AudioStream *, voice_count> voices_{};
  std::array<Uint64, voice_count> voice_started_{};
  std::array<float, voice_count> voice_volume_{};
};

} // namespace phantomcrash
