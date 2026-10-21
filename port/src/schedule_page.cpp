// The schedule's pages and bottom row: the Phantom Crash calendar or
// S.L.A.I.'s (phantomcrash/schedule_slai.hpp), and EXIT, SKIP TO DAY and
// the toggle between them.

#include "phantomcrash/schedule.hpp"

#include <algorithm>

namespace phantomcrash {

int Schedule::rows() const {
  return slai_page_ ? static_cast<int>(slai_->table.display_order.size()) : schedule_venues;
}

Sprite Schedule::button_sprite(const int button, const bool lit) const {
  const SlaiScheduleScript &slai = script_.slai;
  Sprite sprite;
  if (button == toggle_button) {
    sprite = slai_page_ ? (lit ? slai.back_toggle_lit : slai.back_toggle)
                        : (lit ? slai.toggle_lit : slai.toggle);
    sprite.position = slai.toggle_at;
  } else if (button == skip_button) {
    sprite = lit ? script_.skip.button_selected : script_.skip.button;
    sprite.position = toggle_offered() ? slai.skip_at : script_.skip.button.position;
  } else {
    sprite = lit ? script_.exit_selected : script_.exit;
    sprite.position = toggle_offered() ? slai.exit_at : script_.exit.position;
  }
  return sprite;
}

std::vector<int> Schedule::bottom_buttons() const {
  std::vector<int> buttons{exit_button};
  if (skip_offered_) {
    buttons.push_back(skip_button);
  }
  if (toggle_offered()) {
    buttons.push_back(toggle_button);
  }
  std::ranges::sort(buttons, [this](const int left, const int right) {
    return button_sprite(left, false).position[0] < button_sprite(right, false).position[0];
  });
  return buttons;
}

bool Schedule::move_button(const int step) {
  const std::vector<int> buttons = bottom_buttons();
  const auto at = std::ranges::find(buttons, button_);
  if (step == 0 || at == buttons.end()) {
    return false;
  }
  const auto last = static_cast<int>(buttons.size()) - 1;
  const int index = std::clamp(static_cast<int>(at - buttons.begin()) + step, 0, last);
  const int moved = buttons[static_cast<std::size_t>(index)];
  if (moved == button_) {
    return false;
  }
  button_ = moved;
  return true;
}

Sprite Schedule::cell_box(const int column, const int row) const {
  Sprite box;
  const bool slai = slai_page_;
  const auto &origin = slai ? script_.slai.cell_origin : script_.cell_origin;
  const auto &step = slai ? script_.slai.cell_step : script_.cell_step;
  box.rect = slai ? TexelRect{0.0F, 0.0F, step[0], step[1]} : script_.closed;
  box.position = {origin[0] + step[0] * static_cast<float>(column),
                  origin[1] + step[1] * static_cast<float>(row)};
  return box;
}

int Schedule::slai_city(const int row) const {
  return slai_->table.display_order[static_cast<std::size_t>(row)];
}

std::optional<SlaiCityMatch> Schedule::slai_match_at(const int day, const int row) const {
  if (slai_ == nullptr || row < 0 || row >= static_cast<int>(slai_->table.display_order.size())) {
    return std::nullopt;
  }
  auto match = slai_city_match(*slai_, day, slai_city(row), quest_.slai_rumble);
  if (match && match->match == slai_no_match) {
    return std::nullopt;
  }
  return match;
}

std::optional<SlaiCityMatch> Schedule::selected_slai() const {
  if (!slai_page_ || row_ >= button_row()) {
    return std::nullopt;
  }
  return slai_match_at((first_day_ + column_) % calendar_days, row_);
}

} // namespace phantomcrash
