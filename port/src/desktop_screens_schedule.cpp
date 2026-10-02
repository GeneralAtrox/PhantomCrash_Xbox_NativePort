#include "desktop_screens.hpp"
#include "desktop_game_screens.hpp"

namespace phantomcrash {

bool ScreenRunner::tick_schedule(const InputFrame &input, const double seconds) {
  if (!schedule_overlay_) {
    if (name_ == "schedule" || screen_->done() || !screen_->day_clicked(input)) {
      return false;
    }
    schedule_overlay_ = game_->make("schedule", name_, place_before_, quest_);
    schedule_overlay_->set_music(told_music_);
    screen_->set_paused(true);
    rumble_.stop();
    SDL_Log("Schedule shortcut: open from=%s day=%d; origin suspended", name_.c_str(),
            quest_.day + 1);
    return true; // Consume the opening click; never forward it to either screen.
  }

  hear_music(seconds);
  schedule_overlay_->tick(input, seconds);
  for (const auto &sound : schedule_overlay_->take_sounds()) {
    audio_.play_sound(sound, {}, schedule_overlay_->sound_bus());
  }
  if (const auto song = radio_->update(name_, seconds, !audio_.playing())) {
    play_song(*song);
  }
  tell_music(false);
  if (schedule_overlay_->done()) {
    schedule_overlay_.reset();
    screen_->set_paused(false);
    SDL_Log("Schedule shortcut: close to=%s day=%d; same origin resumed", name_.c_str(),
            quest_.day + 1);
  }
  return true; // Closing input must not activate anything on the restored screen.
}

} // namespace phantomcrash
