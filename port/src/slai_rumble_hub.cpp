// Rumble S.L.A.I.'s cursor, popup and entry (phantomcrash/slai_rumble_hub.hpp).

#include "phantomcrash/slai_rumble_hub.hpp"

#include "phantomcrash/money.hpp"
#include "phantomcrash/shop_pointer.hpp"

#include <algorithm>
#include <utility>

namespace phantomcrash {

SlaiRumbleHub::SlaiRumbleHub(SlaiRumbleHubScript script, const SlaiRumbleData &data,
                             const QuestSave &quest, SlaiRobot riding, const bool rentals,
                             std::string back, MessageBoxStyle box)
    : script_(std::move(script)), data_(data), quest_(quest), riding_(std::move(riding)),
      rentals_(rentals), back_(std::move(back)), fade_(script_.fade_in, script_.fade_out),
      rows_repeat_(script_.repeat_delay, script_.repeat_interval), popup_(script_.info.growth),
      box_(std::move(box)) {
  const SlaiRumbleSave &save = quest_.slai_rumble;
  const bool lesson =
      script_.tutorial && !slai_record_cleared(save, data_.world.ladder.tutorial_record);
  for (const SlaiCityMatch &match :
       slai_day_matches(data_, quest_.day, save, lesson ? std::optional<int>{0} : std::nullopt)) {
    const SlaiMatch *found =
        match.match != slai_no_match ? slai_match(data_.table, match.match) : nullptr;
    if (found == nullptr) {
      continue;
    }
    if (listed(match)) {
      matches_.push_back(match);
    } else if (std::ranges::find(script_.exclude_records, found->save_record) !=
               script_.exclude_records.end()) {
      excluded_ = true;
    }
  }
  row_ = matches_.empty() ? exit_row() : 0;
}

bool SlaiRumbleHub::listed(const SlaiCityMatch &match) const {
  const int record = slai_match(data_.table, match.match)->save_record;
  const auto named = [record](const std::vector<int> &list) {
    return std::ranges::find(list, record) != list.end();
  };
  if ((!script_.records.empty() && !named(script_.records)) || named(script_.exclude_records)) {
    return false;
  }
  return !script_.one_per_match ||
         std::ranges::none_of(
             matches_, [&match](const SlaiCityMatch &other) { return other.match == match.match; });
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

// 0x259a40 over the ridden robot. A rental-only rumble refuses it (the
// original's case 0), so there the port lends a robot instead (a port
// path; S.L.A.I.'s rentals are untraced) once the money is held.
void SlaiRumbleHub::enter() {
  const SlaiMatch *match = slai_match(data_.table, selected()->match);
  const SlaiRegulation *rule = slai_regulation(data_.table, match->regulation);
  const std::int64_t requirement = slai_entry_requirement(data_.world, *rule);
  const std::vector<std::string> *lines = nullptr;
  if (rule->special == SlaiSpecialRule::rental_only) {
    lines = quest_.money < requirement ? &script_.short_of_money
            : !rentals_                ? &script_.no_rental
                                       : nullptr;
  } else if (const SlaiEntry entry = slai_entry_check(data_.world, *rule, quest_.money, riding_);
             entry != SlaiEntry::allowed) {
    lines = &slai_refusal_lines(script_, entry);
  }
  if (lines != nullptr) {
    sounds_.play(script_.refuse_sound);
    box_.open(MessageBox::Kind::notice, slai_refusal_notice(data_, *rule, *lines));
    sounds_.play(box_.style().open_sound);
    return;
  }
  sounds_.play(script_.select_sound);
  entering_ = true;
  fade_.leave(script_.battle);
}

} // namespace phantomcrash
