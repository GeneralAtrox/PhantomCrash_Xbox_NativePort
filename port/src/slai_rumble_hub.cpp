// Rumble S.L.A.I.'s cursor, popup and entry (phantomcrash/slai_rumble_hub.hpp).

#include "phantomcrash/slai_rumble_hub.hpp"

#include "phantomcrash/money.hpp"
#include "phantomcrash/rumbling_rules.hpp"
#include "phantomcrash/shop_pointer.hpp"

#include <algorithm>
#include <utility>

namespace phantomcrash {
namespace {

// Every "{entry}" in the lines replaced by the money the rumble needs.
std::vector<std::string> filled(std::vector<std::string> lines, const std::int64_t entry) {
  const std::string key = "{entry}";
  const std::string value = std::to_string(shown_ny(entry));
  for (std::string &line : lines) {
    for (auto at = line.find(key); at != std::string::npos; at = line.find(key, at)) {
      line.replace(at, key.size(), value);
      at += value.size();
    }
  }
  return lines;
}

} // namespace

SlaiRumbleHub::SlaiRumbleHub(SlaiRumbleHubScript script, const SlaiRumbleData &data,
                             const QuestSave &quest, std::string back, MessageBoxStyle box)
    : script_(std::move(script)), data_(data), quest_(quest), back_(std::move(back)),
      fade_(script_.fade_in, script_.fade_out),
      rows_repeat_(script_.repeat_delay, script_.repeat_interval), popup_(script_.info.growth),
      box_(std::move(box)) {
  for (const SlaiCityMatch &match : slai_day_matches(data_, quest_.day, quest_.slai_rumble)) {
    if (match.match != slai_no_match && slai_match(data_.table, match.match) != nullptr) {
      matches_.push_back(match);
    }
  }
  row_ = matches_.empty() ? exit_row() : 0;
}

const SlaiCityMatch *SlaiRumbleHub::selected() const {
  return row_ < exit_row() ? &matches_[static_cast<std::size_t>(row_)] : nullptr;
}

std::optional<SlaiCityMatch> SlaiRumbleHub::entered() const {
  if (!entering_ || selected() == nullptr) {
    return std::nullopt;
  }
  return *selected();
}

Sprite SlaiRumbleHub::row_box(const int row) const {
  Sprite box;
  const auto &cursor = script_.cursor_box;
  box.rect = {0.0F, 0.0F, cursor[2] - cursor[0], cursor[3] - cursor[1]};
  box.position = {script_.row_origin[0] + script_.row_step[0] * static_cast<float>(row) + cursor[0],
                  script_.row_origin[1] + script_.row_step[1] * static_cast<float>(row) +
                      cursor[1]};
  return box;
}

void SlaiRumbleHub::update(const double tick_seconds, const InputFrame &input) {
  seconds_ += tick_seconds;
  popup_.update(tick_seconds);
  box_.update(tick_seconds);
  if (fade_.update(tick_seconds) && !fade_.leaving()) {
    handle_input(tick_seconds, input);
  }
}

void SlaiRumbleHub::handle_input(const double tick_seconds, InputFrame input) {
  if (box_.is_open()) {
    // The notice's OK, or cancel, closes it.
    const bool ok = input.clicked && box_.grown() && shop_hit(input, box_.style().ok.normal);
    if (box_.grown() && (ok || input.any_pressed(script_.confirm_buttons) ||
                         input.any_pressed(script_.cancel_buttons))) {
      sounds_.play(script_.close_sound);
      box_.close();
    }
    return;
  }
  if (input.clicked && input.pointer) {
    if (popup_.is_open()) {
      const bool inside = shop_hit(input, script_.info.panel);
      input.pressed |= inside ? script_.confirm_buttons : script_.cancel_buttons;
    } else if (shop_hit(input, script_.exit)) {
      row_ = exit_row();
      input.pressed |= script_.confirm_buttons;
    } else {
      for (int row = 0; row < exit_row(); ++row) {
        if (shop_hit(input, row_box(row))) {
          row_ = row;
          input.pressed |= script_.confirm_buttons;
        }
      }
    }
  }
  if (popup_.is_open()) {
    if (input.any_pressed(script_.cancel_buttons)) {
      sounds_.play(script_.close_sound);
      popup_.close();
    } else if (popup_.grown() && input.any_pressed(script_.confirm_buttons)) {
      enter();
    }
    return;
  }
  if (input.any_pressed(script_.confirm_buttons)) {
    sounds_.play(script_.select_sound);
    if (row_ == exit_row()) {
      fade_.leave(back_);
    } else {
      popup_.open();
    }
    return;
  }
  if (input.any_pressed(script_.cancel_buttons)) {
    if (row_ != exit_row()) {
      sounds_.play(script_.move_sound);
    }
    row_ = exit_row();
    return;
  }
  bool fresh = false;
  if (const int step = rows_repeat_.step(tick_seconds, input, script_.up, script_.down, fresh)) {
    const int moved = move_cursor(row_, step, fresh, exit_row() + 1);
    if (moved != row_) {
      sounds_.play(script_.move_sound);
      row_ = moved;
    }
  }
}

// What the port's robot can answer of 0x259a40's check: the money held and
// a ridden robot that is ready. Its maker, legs and weapons wait for the
// S.L.A.I. robot adapter (SLAI_RUMBLE_PLAN.md, Status).
void SlaiRumbleHub::enter() {
  const SlaiMatch *match = slai_match(data_.table, selected()->match);
  const SlaiRegulation *rule = slai_regulation(data_.table, match->regulation);
  const std::int64_t requirement = slai_entry_requirement(data_.world, *rule);
  const bool money = quest_.money >= requirement;
  if (!money || !riding_ready(quest_)) {
    sounds_.play(script_.refuse_sound);
    box_.open(MessageBox::Kind::notice,
              filled(money ? script_.not_ready : script_.short_of_money, requirement));
    sounds_.play(box_.style().open_sound);
    return;
  }
  sounds_.play(script_.select_sound);
  entering_ = true;
  fade_.leave(script_.battle);
}

} // namespace phantomcrash
