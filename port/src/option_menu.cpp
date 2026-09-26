#include "phantomcrash/option_menu.hpp"

#include <utility>

namespace phantomcrash {

OptionMenu::OptionMenu(OptionScript script, MessageBoxStyle box, const GameOptions options,
                       const std::string &returning_from)
    : script_(std::move(script)), box_(std::move(box)), options_(options),
      fade_(script_.fade_in, script_.fade_out), help_(script_.items.size()),
      repeat_(script_.repeat_delay, script_.repeat_interval) {
  cursor_ = script_.default_cursor;
  // Back from a screen an item led to, the cursor is on that item (the
  // original picks it from the scene it returns from).
  for (std::size_t index = 0; index < script_.items.size(); ++index) {
    const auto &item = script_.items[index];
    if (item.action == OptionAction::go_to && item.go_to == returning_from) {
      cursor_ = static_cast<int>(index);
    }
    if (item.action == OptionAction::reset) {
      reset_choice_ = item.default_choice;
    }
  }
}

std::vector<std::string> OptionMenu::take_sounds() {
  return std::exchange(sounds_, {});
}

void OptionMenu::sound(const std::string &name) {
  if (!name.empty()) {
    sounds_.push_back(name);
  }
}

void OptionMenu::open_box(const MessageBox::Kind kind, const std::vector<std::string> &lines,
                          const int choice) {
  // The original grows a box only the first time it opens in the scene; the
  // port grows it, with its sound, every time.
  box_.open(kind, lines, choice);
  sound(box_.style().open_sound);
  phase_ = kind == MessageBox::Kind::question ? Phase::question : Phase::notice;
  repeat_.reset();
}

void OptionMenu::close_box() {
  box_.close();
  phase_ = Phase::list;
  repeat_.reset();
}

void OptionMenu::update(const double tick_seconds, const InputFrame &input) {
  if (done()) {
    return;
  }
  seconds_ += tick_seconds;
  // The help line and the window keep moving in every phase, fades included.
  help_.advance(cursor_, tick_seconds);
  box_.update(tick_seconds);
  // Input waits for the fade in (0x2eb6f) and stops once leaving.
  if (!fade_.update(tick_seconds)) {
    return;
  }
  if (phase_ == Phase::list) {
    update_list(tick_seconds, input);
  } else {
    update_box(tick_seconds, input);
  }
}

void OptionMenu::update_list(const double tick_seconds, const InputFrame &input) {
  const int count = static_cast<int>(script_.items.size());
  bool fresh = false;
  const int step = repeat_.step(tick_seconds, input, button::up, button::down, fresh);
  if (const int moved = move_cursor(cursor_, step, fresh, count); moved != cursor_) {
    cursor_ = moved;
    // The new item's help line starts again from the right.
    help_.restart(cursor_);
    sound(script_.move_sound);
  }

  if (input.any_pressed(script_.confirm_buttons)) {
    sound(script_.select_sound);
    const OptionItem &item = script_.items[static_cast<std::size_t>(cursor_)];
    switch (item.action) {
    case OptionAction::go_to:
      fade_.leave(item.go_to);
      break;
    case OptionAction::exit:
      fade_.leave(script_.exit_to);
      break;
    case OptionAction::toggle: {
      // YES stands for on; the box opens on the current setting.
      const bool on = *game_option(options_, item.option);
      open_box(MessageBox::Kind::question, item.question, on ? 0 : 1);
      break;
    }
    case OptionAction::reset:
      // The answer given last time in this screen is remembered.
      open_box(MessageBox::Kind::question, item.question, reset_choice_);
      break;
    }
    return;
  }
  // Cancel does not leave: it moves the cursor to EXIT, whose help line
  // carries on from where it was.
  if (input.any_pressed(script_.cancel_buttons) && cursor_ != script_.cancel_cursor) {
    cursor_ = script_.cancel_cursor;
    sound(script_.move_sound);
  }
}

void OptionMenu::update_box(const double tick_seconds, const InputFrame &input) {
  const bool question = phase_ == Phase::question;
  if (question) {
    bool fresh = false;
    const int step = repeat_.step(tick_seconds, input, button::left, button::right, fresh);
    if (const int moved = move_cursor(box_.choice(), step, fresh, 2); moved != box_.choice()) {
      box_.set_choice(moved);
      sound(script_.move_sound);
    }
  }

  if (input.any_pressed(script_.confirm_buttons)) {
    sound(script_.select_sound);
    const OptionItem &item = script_.items[static_cast<std::size_t>(cursor_)];
    const bool yes = box_.choice() == 0;
    if (!question) {
      close_box();
    } else if (item.action == OptionAction::toggle) {
      *game_option(options_, item.option) = yes;
      close_box();
    } else {
      reset_choice_ = box_.choice();
      if (yes) {
        // Every option: vibration, sound, buttons and screen (0x2e6ea).
        options_ = GameOptions{};
        open_box(MessageBox::Kind::notice, item.done, 0);
      } else {
        close_box();
      }
    }
    return;
  }
  // Cancel moves the choice to NO; a notice ignores it.
  if (question && input.any_pressed(script_.cancel_buttons) && box_.choice() != 1) {
    box_.set_choice(1);
    sound(script_.move_sound);
  }
}

} // namespace phantomcrash
