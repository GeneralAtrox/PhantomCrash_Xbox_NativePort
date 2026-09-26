#include "desktop_audio.hpp"

#include <algorithm>
#include <cstring>

namespace phantomcrash {
namespace {

constexpr int music_chunk_bytes = 64 * 1024;

std::uint32_t read_u32(const std::uint8_t *bytes) {
  return bytes[0] | (bytes[1] << 8) | (bytes[2] << 16) |
         (static_cast<std::uint32_t>(bytes[3]) << 24);
}

std::uint16_t read_u16(const std::uint8_t *bytes) {
  return static_cast<std::uint16_t>(bytes[0] | (bytes[1] << 8));
}

// Finds the format and the sample data of a RIFF WAV file. Supports PCM
// (8 and 16 bit) and 32-bit float, which covers the game's music.
bool parse_wav(SDL_IOStream *file, SDL_AudioSpec &spec, Sint64 &start, Sint64 &end) {
  std::uint8_t header[12];
  if (SDL_ReadIO(file, header, sizeof(header)) != sizeof(header) ||
      std::memcmp(header, "RIFF", 4) != 0 || std::memcmp(header + 8, "WAVE", 4) != 0) {
    return false;
  }
  bool have_format = false;
  std::uint8_t chunk[8];
  while (SDL_ReadIO(file, chunk, sizeof(chunk)) == sizeof(chunk)) {
    const std::uint32_t size = read_u32(chunk + 4);
    const Sint64 body = SDL_TellIO(file);
    if (std::memcmp(chunk, "fmt ", 4) == 0 && size >= 16) {
      std::uint8_t format[16];
      if (SDL_ReadIO(file, format, sizeof(format)) != sizeof(format)) {
        return false;
      }
      const auto tag = read_u16(format);
      const auto bits = read_u16(format + 14);
      spec.channels = read_u16(format + 2);
      spec.freq = static_cast<int>(read_u32(format + 4));
      if (tag == 1 && bits == 16) {
        spec.format = SDL_AUDIO_S16LE;
      } else if (tag == 1 && bits == 8) {
        spec.format = SDL_AUDIO_U8;
      } else if (tag == 3 && bits == 32) {
        spec.format = SDL_AUDIO_F32LE;
      } else {
        return false;
      }
      have_format = true;
    } else if (std::memcmp(chunk, "data", 4) == 0) {
      start = body;
      end = body + size;
      return have_format;
    }
    SDL_SeekIO(file, body + size + (size & 1), SDL_IO_SEEK_SET);
  }
  return false;
}

} // namespace

AudioMixer::AudioMixer(const DataRoots &roots, const json::JsonValue &music,
                       const json::JsonValue &effects)
    : roots_(roots) {
  if (const auto *tracks = music.find("tracks")) {
    for (const auto &[name, value] : tracks->object()) {
      tracks_[name] = {value.string_or("file", ""), value.boolean_or("loop", true),
                       static_cast<float>(value.number_or("volume", 1.0))};
    }
  }
  if (const auto *list = effects.find("effects")) {
    for (const auto &[name, value] : list->object()) {
      effect_files_[name] = {value.string_or("file", ""),
                             static_cast<float>(value.number_or("volume", 1.0))};
    }
  }
  device_ = SDL_OpenAudioDevice(SDL_AUDIO_DEVICE_DEFAULT_PLAYBACK, nullptr);
  if (device_ == 0) {
    SDL_Log("No audio device for music and effects: %s", SDL_GetError());
    return;
  }
  const SDL_AudioSpec initial{SDL_AUDIO_S16LE, 1, 44100};
  for (auto &voice : voices_) {
    voice = SDL_CreateAudioStream(&initial, nullptr);
    if (voice != nullptr) {
      SDL_BindAudioStream(device_, voice);
    }
  }
}

AudioMixer::~AudioMixer() {
  stop_music();
  for (auto *voice : voices_) {
    SDL_DestroyAudioStream(voice);
  }
  if (device_ != 0) {
    SDL_CloseAudioDevice(device_);
  }
}

void AudioMixer::stop_music() {
  SDL_DestroyAudioStream(music_);
  music_ = nullptr;
  if (music_file_ != nullptr) {
    SDL_CloseIO(music_file_);
    music_file_ = nullptr;
  }
}

bool AudioMixer::open_music(const Track &track) {
  const auto path = roots_.find_asset(track.file);
  if (!path) {
    SDL_Log("Music %s not found", track.file.c_str());
    return false;
  }
  music_file_ = SDL_IOFromFile(path->string().c_str(), "rb");
  SDL_AudioSpec spec{};
  if (music_file_ == nullptr || !parse_wav(music_file_, spec, data_start_, data_end_)) {
    SDL_Log("Music %s is not a readable PCM WAV", track.file.c_str());
    return false;
  }
  music_ = SDL_CreateAudioStream(&spec, nullptr);
  if (music_ == nullptr || !SDL_BindAudioStream(device_, music_)) {
    SDL_Log("Music stream failed: %s", SDL_GetError());
    return false;
  }
  music_track_volume_ = track.volume;
  SDL_SetAudioStreamGain(music_, music_track_volume_ * music_gain_);
  music_loop_ = track.loop;
  music_bytes_per_second_ = SDL_AUDIO_FRAMESIZE(spec) * spec.freq;
  SDL_SeekIO(music_file_, data_start_, SDL_IO_SEEK_SET);
  return true;
}

void AudioMixer::play_music(const std::string &track) {
  if (track == track_) {
    return;
  }
  track_ = track;
  stop_music();
  if (track.empty() || device_ == 0) {
    return;
  }
  const auto found = tracks_.find(track);
  if (found == tracks_.end()) {
    SDL_Log("No music track named %s", track.c_str());
    return;
  }
  if (!open_music(found->second)) {
    stop_music();
    return;
  }
  update();
}

void AudioMixer::update() {
  if (music_ == nullptr) {
    return;
  }
  // Half a second queued ahead is plenty and keeps a track change prompt.
  std::vector<std::uint8_t> buffer(music_chunk_bytes);
  while (SDL_GetAudioStreamQueued(music_) < music_bytes_per_second_ / 2) {
    Sint64 position = SDL_TellIO(music_file_);
    if (position >= data_end_) {
      if (!music_loop_) {
        SDL_FlushAudioStream(music_);
        return;
      }
      SDL_SeekIO(music_file_, data_start_, SDL_IO_SEEK_SET);
      position = data_start_;
    }
    const auto wanted = static_cast<std::size_t>(
        std::min<Sint64>(static_cast<Sint64>(buffer.size()), data_end_ - position));
    const std::size_t got = SDL_ReadIO(music_file_, buffer.data(), wanted);
    if (got == 0) {
      SDL_Log("Music read failed; stopping it");
      stop_music();
      return;
    }
    SDL_PutAudioStreamData(music_, buffer.data(), static_cast<int>(got));
  }
}

AudioMixer::Effect *AudioMixer::effect(const std::string &name) {
  auto &loaded = effects_[name];
  if (loaded.loaded) {
    return loaded.samples.empty() ? nullptr : &loaded;
  }
  loaded.loaded = true;
  std::pair<std::string, float> source{name, 1.0F};
  if (name.find('/') == std::string::npos) {
    const auto file = effect_files_.find(name);
    if (file == effect_files_.end()) {
      SDL_Log("No sound effect named %s", name.c_str());
      return nullptr;
    }
    source = file->second;
  }
  loaded.volume = source.second;
  const auto path = roots_.find_asset(source.first);
  Uint8 *samples = nullptr;
  Uint32 length = 0;
  if (!path || !SDL_LoadWAV(path->string().c_str(), &loaded.spec, &samples, &length)) {
    SDL_Log("Sound effect %s could not be loaded", source.first.c_str());
    return nullptr;
  }
  loaded.samples.assign(samples, samples + length);
  SDL_free(samples);
  return &loaded;
}

void AudioMixer::play_sound(const std::string &name) {
  Effect *sound = device_ != 0 ? effect(name) : nullptr;
  if (sound == nullptr) {
    return;
  }
  // An idle voice, or else the one that started longest ago.
  std::size_t chosen = 0;
  for (std::size_t index = 0; index < voice_count; ++index) {
    if (voices_[index] != nullptr && SDL_GetAudioStreamQueued(voices_[index]) == 0) {
      chosen = index;
      break;
    }
    if (voice_started_[index] < voice_started_[chosen]) {
      chosen = index;
    }
  }
  SDL_AudioStream *voice = voices_[chosen];
  if (voice == nullptr) {
    return;
  }
  voice_started_[chosen] = SDL_GetTicksNS();
  voice_volume_[chosen] = sound->volume;
  SDL_ClearAudioStream(voice);
  SDL_SetAudioStreamFormat(voice, &sound->spec, nullptr);
  SDL_SetAudioStreamGain(voice, sound->volume * effect_gain_);
  SDL_PutAudioStreamData(voice, sound->samples.data(), static_cast<int>(sound->samples.size()));
  SDL_FlushAudioStream(voice);
}

void AudioMixer::set_volumes(const float music, const float effects) {
  if (music != music_gain_) {
    music_gain_ = music;
    if (music_ != nullptr) {
      SDL_SetAudioStreamGain(music_, music_track_volume_ * music_gain_);
    }
  }
  if (effects != effect_gain_) {
    effect_gain_ = effects;
    for (std::size_t index = 0; index < voice_count; ++index) {
      if (voices_[index] != nullptr) {
        SDL_SetAudioStreamGain(voices_[index], voice_volume_[index] * effect_gain_);
      }
    }
  }
}

void AudioMixer::set_paused(const bool paused) {
  if (device_ == 0) {
    return;
  }
  if (paused) {
    SDL_PauseAudioDevice(device_);
  } else {
    SDL_ResumeAudioDevice(device_);
  }
}

} // namespace phantomcrash
