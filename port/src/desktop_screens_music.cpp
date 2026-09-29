// The runner's music: the track a screen names, the volumes, the radio's
// songs and the spectrum the screens show.

#include "desktop_screens.hpp"

#include "phantomcrash/sound_config.hpp"

#include <utility>

namespace phantomcrash {

std::string ScreenRunner::music_file(const std::string &track) const {
  if (track == "keep") {
    return audio_.music() == "keep" ? std::string{} : music_file(audio_.music());
  }
  if (track.find('/') != std::string::npos) {
    return track;
  }
  const auto file = track_files_.find(track);
  return file == track_files_.end() ? std::string{} : file->second;
}

void ScreenRunner::apply_volumes() {
  audio_.set_volumes(volume_gain(options_.bgm_volume, volume_millibels_),
                     volume_gain(options_.se_volume, volume_millibels_));
}

void ScreenRunner::play_song(const std::string &file) {
  audio_.play_music({});
  audio_.play_music(file, radio_->volume(), false);
}

void ScreenRunner::tell_music(const bool always) {
  std::string file = audio_.playing() ? music_file(audio_.music()) : std::string{};
  if (always || file != told_music_) {
    told_music_ = std::move(file);
    screen_->set_music(told_music_);
  }
}

void ScreenRunner::play_music() {
  if (const std::string music = screen_->music(); music != "keep") {
    audio_.play_music(music, screen_->music_volume());
  }
}

void ScreenRunner::hear_music(const double tick_seconds) {
  int rate = 0;
  if (audio_.music_heard(heard_samples_, rate)) {
    spectrum_.hear(heard_samples_, rate, tick_seconds);
  } else {
    spectrum_.silence();
  }
  screen_->hear_music(spectrum_.levels());
}

} // namespace phantomcrash
