#include "desktop_audio.hpp"

#include <algorithm>
#include <cstring>

namespace phantomcrash {
namespace {

constexpr int music_chunk_bytes = 64 * 1024;
// Frames of queued music kept for music_heard(): about 3 s, well over the
// queue ahead plus a chunk.
constexpr std::size_t heard_frames = std::size_t{1} << 17U;

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
    : roots_(roots), heard_(heard_frames) {
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
  open_effects();
}

AudioMixer::~AudioMixer() {
  stop_music();
  SDL_DestroyAudioStream(effects_stream_);
  if (device_ != 0) {
    SDL_CloseAudioDevice(device_);
  }
}

void AudioMixer::stop_music() {
  SDL_DestroyAudioStream(music_);
  music_ = nullptr;
  queued_frames_ = 0;
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
  music_track_volume_ = track.volume * music_volume_;
  SDL_SetAudioStreamGain(music_, music_track_volume_ * music_gain_);
  music_loop_ = track.loop;
  music_bytes_per_second_ = SDL_AUDIO_FRAMESIZE(spec) * spec.freq;
  music_spec_ = spec;
  SDL_SeekIO(music_file_, data_start_, SDL_IO_SEEK_SET);
  return true;
}

void AudioMixer::play_music(const std::string &track, const float volume, const bool loop) {
  if (track == track_) {
    return;
  }
  track_ = track;
  music_volume_ = volume;
  stop_music();
  if (track.empty() || device_ == 0) {
    return;
  }
  Track file{track, loop, 1.0F};
  if (track.find('/') == std::string::npos) {
    const auto found = tracks_.find(track);
    if (found == tracks_.end()) {
      SDL_Log("No music track named %s", track.c_str());
      return;
    }
    file = found->second;
  }
  if (!open_music(file)) {
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
        if (SDL_GetAudioStreamQueued(music_) == 0) {
          stop_music();
        }
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
    keep_heard(buffer.data(), got);
  }
}

void AudioMixer::keep_heard(const std::uint8_t *bytes, const std::size_t size) {
  const int channels = std::max(music_spec_.channels, 1);
  const auto frame = static_cast<std::size_t>(SDL_AUDIO_FRAMESIZE(music_spec_));
  if (frame == 0) {
    return;
  }
  const std::size_t sample_bytes = frame / static_cast<std::size_t>(channels);
  for (std::size_t at = 0; at + frame <= size; at += frame) {
    float sum = 0.0F;
    for (int channel = 0; channel < channels; ++channel) {
      const std::uint8_t *sample = bytes + at + static_cast<std::size_t>(channel) * sample_bytes;
      if (music_spec_.format == SDL_AUDIO_S16LE) {
        sum += static_cast<float>(static_cast<std::int16_t>(read_u16(sample))) / 32768.0F;
      } else if (music_spec_.format == SDL_AUDIO_U8) {
        sum += (static_cast<float>(*sample) - 128.0F) / 128.0F;
      } else {
        float value = 0.0F;
        std::memcpy(&value, sample, sizeof(value));
        sum += value;
      }
    }
    heard_[queued_frames_++ & (heard_frames - 1)] = sum / static_cast<float>(channels);
  }
}

bool AudioMixer::music_heard(std::vector<float> &samples, int &rate) const {
  if (music_ == nullptr || music_track_volume_ * music_gain_ <= 0.0F) {
    return false;
  }
  // What is heard now is the last frame queued less those still waiting.
  const auto waiting = static_cast<std::uint64_t>(std::max(SDL_GetAudioStreamQueued(music_), 0)) /
                       static_cast<std::uint64_t>(SDL_AUDIO_FRAMESIZE(music_spec_));
  const std::uint64_t now = queued_frames_ - std::min(waiting, queued_frames_);
  const std::uint64_t count = std::min<std::uint64_t>(samples.size(), heard_frames);
  for (std::uint64_t index = 0; index < count; ++index) {
    const std::uint64_t at = now - count + index; // wraps below 0 before the track's start
    samples[index] = at < now ? heard_[at & (heard_frames - 1)] : 0.0F;
  }
  rate = music_spec_.freq;
  return true;
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
    if (effects_stream_ != nullptr) {
      SDL_SetAudioStreamGain(effects_stream_, effect_gain_);
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
