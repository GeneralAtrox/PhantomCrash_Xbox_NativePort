#pragma once

#include "desktop_town_screen.hpp"
#include "phantomcrash/schedule.hpp"

#include <array>
#include <string>
#include <vector>

namespace phantomcrash {

class DrawList;

// A S.L.A.I. rumble's details in `info`'s places (its panel is the
// caller's): the regulation's name, `class_text`, the rule it sets on the
// body, the legs or the weapons (its name again, S.L.A.I.'s own text) or
// "No limits.", and the money entry needs. The schedule's S.L.A.I. page and
// Rumble S.L.A.I. show it.
void draw_slai_rule_info(DrawList &list, const GlyphFont &font, const NumberFont &fee_font,
                         const ContestInfoStyle &info, const SlaiRumbleData &data,
                         const SlaiRegulation &rule, const std::string &class_text);

// A frame cut into nine: corners `border` square kept, edges and middle
// stretched over `size` at `at`.
void nine_slice(DrawList &list, const Sprite &sprite, const std::array<float, 2> &at,
                const std::array<float, 2> &size, float border);

// Folds a sprite about its centre to `scale_x` of its width, as a column
// of the calendar folds while it scrolls.
void fold(Sprite &sprite, float scale_x);

// The contest schedule (Schedule): the calendar's cells, the cursor or
// EXIT, the contest's name running along the top, the INFORMATION popup and
// the town frame; with S.L.A.I., its page of the same days. The contest
// tables and the S.L.A.I. data outlive the screen.
class ScheduleScreen final : public TownScreen {
public:
  ScheduleScreen(TextureCache &textures, const GlyphFont &font, ScheduleScript script,
                 const ContestTables &contests, const std::string &from, const std::string &back,
                 const FrameStyle &frame, const std::vector<MusicCredit> &credits, QuestSave quest,
                 const MessageBoxStyle &box, const SlaiRumbleData *slai = nullptr);

  void tick(const InputFrame &input, double tick_seconds) override;
  void draw(const Viewport &viewport) override;
  [[nodiscard]] std::vector<std::string> take_sounds() override {
    return schedule_.take_sounds();
  }
  [[nodiscard]] bool done() const override {
    return schedule_.done();
  }
  [[nodiscard]] std::string next() const override {
    return schedule_.next();
  }
  // Set only once SKIP TO DAY leaves to sleep the last night.
  [[nodiscard]] const QuestSave *quest_save() const override {
    return schedule_.skipped_quest();
  }

private:
  void draw_cells(DrawList &list) const;
  void draw_days(DrawList &list) const;
  void draw_cursor(DrawList &list) const;
  void draw_popup(DrawList &list) const;
  // SKIP TO DAY beside EXIT, its question, and the opponents' level raise.
  void draw_skip(DrawList &list) const;
  // The top-left of a cell on the canvas.
  [[nodiscard]] std::array<float, 2> cell(int column, int row) const;
  // The bottom row's buttons, the lit one lit (desktop_schedule.cpp).
  void draw_buttons(DrawList &list) const;
  // The arrow lit on the side the calendar scrolls towards.
  void draw_scroll_arrow(DrawList &list) const;

  // The S.L.A.I. page (desktop_schedule_slai.cpp): the DAY- strip and the
  // days, each city's plate and cells, the grid's lines, the cursor and the
  // rumble's INFORMATION.
  void draw_slai_days(DrawList &list) const;
  void draw_slai_cells(DrawList &list) const;
  void draw_slai_plates(DrawList &list) const;
  void draw_slai_cursor(DrawList &list) const;
  void draw_slai_info(DrawList &list) const;
  // The name running along the top on the S.L.A.I. page.
  [[nodiscard]] std::string slai_ticker_text() const;

  const ContestTables &contests_;
  const NumberFont &fee_font_;
  Schedule schedule_;
};

} // namespace phantomcrash
