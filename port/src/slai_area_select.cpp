// S.L.A.I.'s area select (phantomcrash/slai_area_select.hpp), following
// RUMBLING's (rumbling.cpp, rumbling_gates.cpp) step for step.

#include "phantomcrash/slai_area_select.hpp"

#include <algorithm>
#include <utility>

namespace phantomcrash {
namespace {

float progress(const double seconds, const double span) {
  return span > 0.0 ? static_cast<float>(std::clamp(seconds / span, 0.0, 1.0)) : 1.0F;
}

} // namespace

SlaiAreaSelect::SlaiAreaSelect(SlaiAreaSelectScript script, const SlaiRumbleData &data,
                               const SlaiAreaMaps &maps, QuestSave quest, SlaiRobot riding,
                               const bool rentals, const std::optional<int> resume_area)
    : TownLogic(script.rumbling), script_(std::move(script)), data_(data), quest_(std::move(quest)),
      riding_(std::move(riding)), rentals_(rentals),
      across_(script_.rumbling.repeat_delay, script_.rumbling.repeat_interval),
      rows_(script_.rumbling.repeat_delay, script_.rumbling.repeat_interval) {
  messages_ = script_.rumbling.messages;
  fade_seconds_ = script_.rumbling.area_select.fade_seconds;
  build_areas(maps);
  const int exit = static_cast<int>(held_.size()) - 1;
  if (resume_area && *resume_area >= 0 && *resume_area < exit && held(*resume_area)) {
    // As RUMBLING's, back from the hangar: the area's sub-menu on SELECT SV.
    area_ = faded_area_ = *resume_area;
    const RumblingAreaSelect &style = script_.rumbling.area_select;
    open_seconds_ = style.slide_seconds + style.bar_slide_seconds;
    turn_seconds_ = script_.rumbling.menu.turn_seconds;
    item_ = static_cast<int>(RumblingAction::select_sv);
    view_ = RumblingView::menu;
  }
  refresh_help();
}

float SlaiAreaSelect::area_fade() const {
  return progress(fade_seconds_, script_.rumbling.area_select.fade_seconds);
}

float SlaiAreaSelect::items_slide() const {
  return progress(open_seconds_, script_.rumbling.area_select.slide_seconds);
}

float SlaiAreaSelect::bar_slide() const {
  return progress(open_seconds_, script_.rumbling.area_select.bar_slide_seconds);
}

float SlaiAreaSelect::turn() const {
  return progress(turn_seconds_, script_.rumbling.menu.turn_seconds);
}

bool SlaiAreaSelect::in_menu() const noexcept {
  return view_ == RumblingView::menu || view_ == RumblingView::gates ||
         view_ == RumblingView::contest_info ||
         (view_ == RumblingView::notice && back_view_ == RumblingView::menu);
}

int SlaiAreaSelect::gate_count() const {
  return static_cast<int>(script_.rumbling.areas[static_cast<std::size_t>(area_)].gates.size());
}

void SlaiAreaSelect::set_help(const std::string &text) {
  if (help_ != text) {
    help_ = text;
    help_seconds_ = 0.0;
  }
}

void SlaiAreaSelect::refresh_help() {
  const RumblingScript &rumbling = script_.rumbling;
  switch (view_) {
  case RumblingView::areas:
  case RumblingView::turning_out:
    set_help(rumbling.areas[static_cast<std::size_t>(area_)].help);
    break;
  case RumblingView::turning_in:
  case RumblingView::menu:
  case RumblingView::character_info:
    set_help(rumbling.menu.items[static_cast<std::size_t>(item_)].help);
    break;
  case RumblingView::gates:
    set_help(rumbling.gates.help);
    break;
  case RumblingView::contest_info:
    set_help(rumbling.contest_info.help);
    break;
  case RumblingView::notice:
    break;
  }
}

void SlaiAreaSelect::say(const std::string &message, const RumblingView back) {
  message_ = message;
  back_view_ = back;
  view_ = RumblingView::notice;
}

// S.L.A.I.'s own notice, filled for the rumble.
void SlaiAreaSelect::refuse(const std::vector<std::string> &lines, const RumblingView back) {
  if (const SlaiRegulation *shown = rule()) {
    messages_["slai"] = slai_refusal_notice(data_, *shown, lines);
    say("slai", back);
  }
}

void SlaiAreaSelect::update(const double tick_seconds, const InputFrame &input) {
  if (done()) {
    return;
  }
  help_seconds_ += tick_seconds;
  open_seconds_ += tick_seconds;
  fade_seconds_ += tick_seconds;
  menu_seconds_ += tick_seconds;
  info_seconds_ += tick_seconds;
  const double turn_span = script_.rumbling.menu.turn_seconds;
  if (view_ == RumblingView::turning_in) {
    turn_seconds_ = std::min(turn_seconds_ + tick_seconds, turn_span);
    if (turn_seconds_ >= turn_span) {
      view_ = RumblingView::menu;
      menu_seconds_ = 0.0;
    }
  } else if (view_ == RumblingView::turning_out) {
    turn_seconds_ = std::max(turn_seconds_ - tick_seconds, 0.0);
    if (turn_seconds_ <= 0.0) {
      view_ = RumblingView::areas;
    }
  }
  if (!advance(tick_seconds)) {
    return;
  }
  const RumblingScript &rumbling = script_.rumbling;
  switch (view_) {
  case RumblingView::areas:
    areas_input(tick_seconds, input);
    break;
  case RumblingView::menu:
    menu_input(tick_seconds, input);
    break;
  case RumblingView::gates:
    gates_input(tick_seconds, input);
    break;
  case RumblingView::contest_info:
    if (input.any_pressed(rumbling.cancel_buttons)) {
      sound(rumbling.close_sound);
      view_ = RumblingView::menu;
    }
    break;
  case RumblingView::notice:
    if (input.any_pressed(rumbling.confirm_buttons)) {
      sound(rumbling.close_sound);
      message_.clear();
      view_ = back_view_;
    }
    break;
  default:
    break;
  }
  refresh_help();
}

void SlaiAreaSelect::move_area(const int area) {
  if (area == area_) {
    return;
  }
  faded_area_ = area_;
  area_ = area;
  fade_seconds_ = 0.0;
}

// As RUMBLING's 0x0163e7: left and right choose the area, wrapping; cancel
// moves to EXIT. A city refuses entry short of its rumble's money.
void SlaiAreaSelect::areas_input(const double tick_seconds, const InputFrame &input) {
  const RumblingScript &rumbling = script_.rumbling;
  const int count = static_cast<int>(rumbling.areas.size());
  bool fresh = false;
  if (const int step = across_.step(tick_seconds, input, rumbling.left, rumbling.right, fresh)) {
    const int moved = move_cursor(area_, step, fresh, count);
    if (moved != area_) {
      sound(rumbling.move_sound);
      move_area(moved);
    }
    return;
  }
  if (input.any_pressed(rumbling.cancel_buttons)) {
    if (area_ != count - 1) {
      sound(rumbling.to_exit_sound);
      move_area(count - 1);
    }
    return;
  }
  if (!input.any_pressed(rumbling.confirm_buttons)) {
    return;
  }
  if (rumbling.areas[static_cast<std::size_t>(area_)].exit) {
    sound(rumbling.select_sound);
    leave(rumbling.exit_to);
    return;
  }
  const SlaiRegulation *shown = rule();
  if (shown == nullptr) {
    sound(rumbling.no_sound);
    return;
  }
  if (quest_.money < slai_entry_requirement(data_.world, *shown)) {
    sound(rumbling.no_sound);
    refuse(script_.hub.short_of_money, RumblingView::areas);
    return;
  }
  sound(rumbling.select_sound);
  item_ = 0;
  view_ = RumblingView::turning_in;
}

// As RUMBLING's 0x01671e: up and down choose; cancel moves to BACK.
void SlaiAreaSelect::menu_input(const double tick_seconds, const InputFrame &input) {
  const RumblingScript &rumbling = script_.rumbling;
  const int count = static_cast<int>(rumbling.menu.items.size());
  bool fresh = false;
  if (const int step = rows_.step(tick_seconds, input, rumbling.up, rumbling.down, fresh)) {
    const int moved = std::clamp(item_ + step, 0, count - 1);
    if (moved != item_) {
      sound(rumbling.row_sound);
      item_ = moved;
    }
    return;
  }
  if (input.any_pressed(rumbling.cancel_buttons)) {
    if (item_ != count - 1) {
      sound(rumbling.to_exit_sound);
      item_ = count - 1;
    }
    return;
  }
  if (input.any_pressed(rumbling.confirm_buttons)) {
    choose(rumbling.menu.items[static_cast<std::size_t>(item_)].action);
  }
}

void SlaiAreaSelect::choose(const RumblingAction action) {
  const RumblingScript &rumbling = script_.rumbling;
  switch (action) {
  case RumblingAction::entry:
    if (can_choose(action)) {
      sound(rumbling.select_sound);
      open_gates(false);
    } else if (rental_only()) {
      sound(rumbling.no_sound);
      say("rentals_only", RumblingView::menu);
    } else {
      sound(rumbling.no_sound);
      refuse(slai_refusal_lines(script_.hub, entry_check()), RumblingView::menu);
    }
    break;
  case RumblingAction::rental:
    if (can_choose(action)) {
      sound(rumbling.select_sound);
      open_gates(true);
    } else if (rental_only()) {
      sound(rumbling.no_sound);
      refuse(script_.hub.no_rental, RumblingView::menu);
    } else {
      sound(rumbling.no_sound);
      say("no_rentals", RumblingView::menu);
    }
    break;
  case RumblingAction::select_sv:
    sound(rumbling.select_sound);
    leave(rumbling.hangar_route);
    break;
  case RumblingAction::character_info:
    // S.L.A.I.'s opponents are drawn as the battle starts: no one to show.
    sound(rumbling.no_sound);
    break;
  case RumblingAction::contest_info:
    sound(rumbling.select_sound);
    info_seconds_ = 0.0;
    view_ = RumblingView::contest_info;
    break;
  case RumblingAction::back:
    view_ = RumblingView::turning_out;
    break;
  }
}

// A stage without a map has no gates to choose: it is entered at once.
void SlaiAreaSelect::open_gates(const bool rental) {
  rental_ = rental;
  const int count = gate_count();
  if (count == 0) {
    gate_ = 0;
    enter();
    return;
  }
  gate_ = std::clamp(gate_, 0, count - 1);
  view_ = RumblingView::gates;
}

// As RUMBLING's 0x0169e5: left and right choose the gate, wrapping; confirm
// enters, cancel goes back to the sub-menu.
void SlaiAreaSelect::gates_input(const double tick_seconds, const InputFrame &input) {
  const RumblingScript &rumbling = script_.rumbling;
  bool fresh = false;
  if (const int step = across_.step(tick_seconds, input, rumbling.left, rumbling.right, fresh)) {
    const int moved = move_cursor(gate_, step, fresh, gate_count());
    if (moved != gate_) {
      sound(rumbling.move_sound);
      gate_ = moved;
    }
    return;
  }
  if (input.any_pressed(rumbling.cancel_buttons)) {
    sound(rumbling.no_sound);
    view_ = RumblingView::menu;
    return;
  }
  if (input.any_pressed(rumbling.confirm_buttons)) {
    sound(rumbling.select_sound);
    enter();
  }
}

void SlaiAreaSelect::enter() {
  entry_ = SlaiAreaEntry{*held(area_), gate_, rental_};
  leave(script_.rumbling.battle_route);
}

} // namespace phantomcrash
