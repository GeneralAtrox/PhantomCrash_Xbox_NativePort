#pragma once

// The contest schedule (the original's scene 32): five days of the contest
// calendar in three venue rows, a cursor over them and EXIT, a scroll a day
// or a page at a time, and a popup with a contest's details. See
// data/ui/schedule.json.

#include "phantomcrash/contest_info.hpp"
#include "phantomcrash/contests.hpp"
#include "phantomcrash/glyph_font.hpp"
#include "phantomcrash/location_frame.hpp"
#include "phantomcrash/menu_cursor.hpp"
#include "phantomcrash/menu_screen.hpp"
#include "phantomcrash/settings.hpp"
#include "phantomcrash/sprite_layer.hpp"
#include "phantomcrash/town_screen.hpp"

#include <array>
#include <cstdint>
#include <string>
#include <vector>

namespace phantomcrash {

using TexelRect = std::array<float, 4>;

struct ScheduleVenueTiles {
  TexelRect day{}, day_lit{}, night{}, night_lit{};
};

struct ScheduleBadge {
  std::vector<int> classes; // empty: every class the others do not name
  TexelRect rect{};
};

struct ScheduleDigits {
  std::string image;
  std::array<TexelRect, 10> digits{};
  std::array<float, 2> tens{}, units{}; // the first column's
  int layer{0x104};
};

struct ScheduleScript : TownBasics {
  Buttons left{}, right{}, up{}, down{}, page_back{}, page_forward{};
  std::string row_sound, page_sound, exit_sound, close_sound;
  Sprite scroll_arrow;
  Sprite exit, exit_selected;
  std::string cell_image; // the tiles, badges and CLEAR
  std::array<float, 2> cell_origin{}, cell_step{};
  int columns{5};
  Sprite cursor_frame, cursor_pulse, cursor_day;
  float cursor_day_y{};
  int tile_layer{8};
  std::vector<ScheduleVenueTiles> venues; // one per row
  TexelRect closed{}, closed_lit{};
  int badge_layer{0x102};
  std::uint32_t badge_dim{0x404040};
  std::vector<ScheduleBadge> badges;
  Sprite clear;
  std::vector<int> no_clear_mark;   // contests won that show no CLEAR
  std::vector<int> hidden_once_won; // contests that close once won
  ScheduleDigits day_digits;
  double scroll_step_seconds{0.05};
  ContestInfoStyle info;
  TickerStyle ticker;
};

// Throws std::runtime_error naming the member at fault.
[[nodiscard]] ScheduleScript parse_schedule(const json::JsonValue &document);

// One column as drawn now: its day, and how a scroll folds or slides it.
struct ScheduleColumn {
  int day{};
  bool shown{true};
  float scale_x{1.0F}; // folding about its centre
  float offset_x{};    // sliding across
};

class Schedule final {
public:
  static constexpr int exit_row = schedule_venues;

  // `back` is where EXIT returns to: the place it was opened from.
  Schedule(ScheduleScript script, const ContestTables &contests, const QuestSave &quest,
           std::string back);

  void update(double tick_seconds, const InputFrame &input);

  [[nodiscard]] const ScheduleScript &script() const noexcept {
    return script_;
  }
  [[nodiscard]] int column() const noexcept {
    return column_;
  }
  [[nodiscard]] int row() const noexcept {
    return row_;
  }
  // The first day of the window the calendar is scrolling to.
  [[nodiscard]] int first_day() const noexcept {
    return first_day_;
  }
  [[nodiscard]] bool scrolling() const noexcept {
    return phase_ != 0;
  }
  // Scrolling backwards (towards earlier days) or forwards.
  [[nodiscard]] int scroll_direction() const noexcept {
    return pending_ < 0 ? -1 : 1;
  }
  [[nodiscard]] ScheduleColumn column_view(int column) const;
  // The contest shown in a cell of the current view, or no_contest; a
  // contest closed once won shows as none.
  [[nodiscard]] int contest_at(int day, int row) const;
  [[nodiscard]] bool won(int contest) const;
  // The contest under the cursor, or no_contest.
  [[nodiscard]] int selected_contest() const;
  [[nodiscard]] double ticker_seconds() const noexcept {
    return ticker_seconds_;
  }
  // The contest's INFORMATION popup; its details show once it has grown.
  [[nodiscard]] const GrowingPopup &popup() const noexcept {
    return popup_;
  }
  [[nodiscard]] bool faded_in() const noexcept {
    return !fade_.fading_in();
  }
  [[nodiscard]] float black_alpha() const noexcept {
    return fade_.black_alpha();
  }
  [[nodiscard]] double seconds() const noexcept {
    return seconds_;
  }
  [[nodiscard]] std::vector<std::string> take_sounds() {
    return sounds_.take();
  }
  [[nodiscard]] bool done() const noexcept {
    return fade_.done();
  }
  [[nodiscard]] std::string next() const {
    return fade_.done() ? fade_.next() : std::string{};
  }

private:
  void handle_input(double tick_seconds, const InputFrame &input);
  void scroll(int days);
  void step_scroll(double tick_seconds);

  ScheduleScript script_;
  const ContestTables &contests_;
  std::vector<int> won_;
  bool game_cleared_{};
  std::string back_;
  ScreenFade fade_;
  CursorRepeat columns_repeat_;
  CursorRepeat rows_repeat_;
  int column_{};
  int row_{};
  int first_day_{};
  int pending_{}; // days still to scroll, signed (0x304)
  int phase_{};   // 0 still, 1 folding, 2 sliding, 3 unfolding (0x2fc)
  double phase_left_{};
  GrowingPopup popup_;
  double ticker_seconds_{};
  double seconds_{};
  SoundQueue sounds_;
};

} // namespace phantomcrash
