#pragma once

// S.L.A.I.'s area select (slai_area_select_script.hpp): RUMBLING's area
// select, sub-menu and gates (rumbling.hpp) over S.L.A.I.'s seven cities,
// each holding the rumble the rota opens there today (slai_rumble_rules.hpp)
// or closed. The checks are S.L.A.I.'s: a city refuses entry with less money
// than its rumble needs; ENTRY is greyed where the ridden robot fails the
// rumble's rules (0x259a40) or the rumble is rental-only, SV RENTAL except
// there. Entering leaves for the battle with the gate chosen; the entry is
// paid as the battle starts (slai_pay_entry).

#include "phantomcrash/input.hpp"
#include "phantomcrash/menu_cursor.hpp"
#include "phantomcrash/rumbling.hpp"
#include "phantomcrash/settings.hpp"
#include "phantomcrash/slai_area_select_script.hpp"
#include "phantomcrash/slai_rumble_rules.hpp"
#include "phantomcrash/town_logic.hpp"

#include <map>
#include <optional>
#include <string>
#include <vector>

namespace phantomcrash {

// The rumble entered: the city's, by the gate chosen (an index into the
// stage's player spawns), in a lent robot when `rental`.
struct SlaiAreaEntry {
  SlaiCityMatch match;
  int gate{};
  bool rental{};
};

class SlaiAreaSelect final : public TownLogic {
public:
  // `riding` is the ridden robot in S.L.A.I.'s terms; `rentals` that a
  // rental-only rumble has robots to lend. `resume_area` comes back from
  // SELECT SV to that area's sub-menu.
  SlaiAreaSelect(SlaiAreaSelectScript script, const SlaiRumbleData &data, const SlaiAreaMaps &maps,
                 QuestSave quest, SlaiRobot riding, bool rentals,
                 std::optional<int> resume_area = std::nullopt);

  void update(double tick_seconds, const InputFrame &input);

  [[nodiscard]] const SlaiAreaSelectScript &script() const noexcept {
    return script_;
  }
  // RUMBLING's script with the cities' areas, by display order, then EXIT.
  [[nodiscard]] const RumblingScript &rumbling() const noexcept {
    return script_.rumbling;
  }
  [[nodiscard]] const SlaiRumbleData &data() const noexcept {
    return data_;
  }
  [[nodiscard]] const QuestSave &quest() const noexcept {
    return quest_;
  }
  // The rumble an area holds today; none for a closed city and EXIT.
  [[nodiscard]] const std::optional<SlaiCityMatch> &held(int area) const;
  // The table's city column an area shows; -1 for EXIT.
  [[nodiscard]] int city(int area) const;
  [[nodiscard]] bool cleared(int area) const;
  [[nodiscard]] RumblingView view() const noexcept {
    return view_;
  }
  [[nodiscard]] int area() const noexcept {
    return area_;
  }
  [[nodiscard]] int faded_area() const noexcept {
    return faded_area_;
  }
  [[nodiscard]] float area_fade() const;
  [[nodiscard]] float items_slide() const;
  [[nodiscard]] float bar_slide() const;
  [[nodiscard]] float turn() const;
  [[nodiscard]] bool in_menu() const noexcept;
  [[nodiscard]] int menu_item() const noexcept {
    return item_;
  }
  [[nodiscard]] double menu_seconds() const noexcept {
    return menu_seconds_;
  }
  [[nodiscard]] bool can_choose(RumblingAction action) const;
  [[nodiscard]] int gate() const noexcept {
    return gate_;
  }
  [[nodiscard]] double info_seconds() const noexcept {
    return info_seconds_;
  }
  // The notice up (a key of messages()), or empty.
  [[nodiscard]] const std::string &message() const noexcept {
    return message_;
  }
  [[nodiscard]] const std::map<std::string, std::vector<std::string>> &messages() const noexcept {
    return messages_;
  }
  [[nodiscard]] const std::string &help_text() const noexcept {
    return help_;
  }
  [[nodiscard]] double help_seconds() const noexcept {
    return help_seconds_;
  }
  [[nodiscard]] const std::optional<SlaiAreaEntry> &entry() const noexcept {
    return entry_;
  }

private:
  void build_areas(const SlaiAreaMaps &maps);
  void areas_input(double tick_seconds, const InputFrame &input);
  void menu_input(double tick_seconds, const InputFrame &input);
  void gates_input(double tick_seconds, const InputFrame &input);
  void choose(RumblingAction action);
  void move_area(int area);
  void open_gates(bool rental);
  void enter();
  void say(const std::string &message, RumblingView back);
  void refuse(const std::vector<std::string> &lines, RumblingView back);
  void set_help(const std::string &text);
  void refresh_help();
  [[nodiscard]] const SlaiRegulation *rule() const;
  [[nodiscard]] bool rental_only() const;
  [[nodiscard]] SlaiEntry entry_check() const;
  [[nodiscard]] int gate_count() const;

  SlaiAreaSelectScript script_;
  const SlaiRumbleData &data_;
  QuestSave quest_;
  SlaiRobot riding_;
  bool rentals_{};
  std::vector<std::optional<SlaiCityMatch>> held_; // by area
  std::vector<int> cities_;                        // by area
  CursorRepeat across_;
  CursorRepeat rows_;
  RumblingView view_{RumblingView::areas};
  RumblingView back_view_{RumblingView::areas};
  int area_{};
  int faded_area_{};
  double fade_seconds_{};
  double open_seconds_{};
  double turn_seconds_{};
  int item_{};
  double menu_seconds_{};
  int gate_{};
  bool rental_{};
  double info_seconds_{};
  std::string message_;
  std::map<std::string, std::vector<std::string>> messages_;
  std::string help_;
  double help_seconds_{};
  std::optional<SlaiAreaEntry> entry_;
};

} // namespace phantomcrash
