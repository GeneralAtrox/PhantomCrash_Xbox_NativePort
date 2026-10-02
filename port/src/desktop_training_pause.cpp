#include "desktop_training.hpp"
#include "desktop_draw_list.hpp"
#include "desktop_shop_dialog.hpp"
#include "phantomcrash/json_fields.hpp"

#include <algorithm>

namespace phantomcrash {

void TrainingScreen::load_pause(const DataRoots &roots) {
  const auto data = roots.document("ui/battle_pause.json");
  const auto result = roots.document("ui/match_result.json");
  pause_exit_ = parse_sprite(json::required(result, "exit_selected"));
  pause_exit_.position = json::numbers<2>(json::required(data, "exit_position"), "exit_position");
  pause_text_ = parse_text_block(json::required(data, "text"));
  pause_dim_ = std::clamp(static_cast<float>(data.number_or("dim_alpha", 0.5)), 0.0F, 1.0F);
  // Existing mods' battle exit binding now opens the pause menu as well.
  pause_buttons_ =
      buttons_from_json(json::required(data, "pause_buttons")) | script_.battle.buttons.exit;
  resume_buttons_ = buttons_from_json(json::required(data, "resume_buttons")) | pause_buttons_;
  pause_exit_button_ = buttons_from_json(json::required(data, "exit_button"));
  static_cast<void>(textures_.get(pause_exit_.image));
}

bool TrainingScreen::tick_pause(const InputFrame &input) {
  if (!paused_) {
    if (done_ || !input.any_pressed(pause_buttons_)) {
      return false;
    }
    paused_ = true;
    SDL_Log("Battle pause menu: clock=%.3f play=%.3f; shared match continues=%d", clock_,
            play_seconds_, coop_match_ != nullptr);
    return true; // Opening input cannot also activate EXIT.
  }
  if (input.any_pressed(resume_buttons_)) {
    paused_ = false;
    jump_armed_ = false;
    SDL_Log("Battle resumed: clock=%.3f play=%.3f", clock_, play_seconds_);
    return true; // Resuming input cannot also fire or move the robot.
  }
  const bool clicked =
      input.clicked && input.pointer &&
      std::ranges::any_of(place_sprite(pause_exit_, 0), [&](const Quad &quad) {
        const auto [x, y] = *input.pointer;
        return x >= quad.x && y >= quad.y && x < quad.x + quad.width && y < quad.y + quad.height;
      });
  if (input.any_pressed(pause_exit_button_) || clicked) {
    paused_ = false;
    done_ = true;
    SDL_Log("Battle pause EXIT: clock=%.3f play=%.3f destination=%s", clock_, play_seconds_,
            next().c_str());
  }
  return true;
}

void TrainingScreen::draw_pause(const Viewport &viewport) {
  if (!paused_) {
    return;
  }
  // Port pause presentation; gameplay and the replay recording stay underneath.
  fill_black(textures_, pause_dim_);
  DrawList list(textures_, font_, viewport, 0);
  list.sprite(pause_exit_);
  draw_text_block(list, font_, pause_text_);
  list.flush();
}

} // namespace phantomcrash
