#pragma once

// Rumble S.L.A.I. (docs/SLAI_RUMBLE_PLAN.md, "Screens"; a port screen):
// Club Wired's way into S.L.A.I.'s campaign. It lists the three cities the
// rota opens today (slai_rota.hpp) with their rumbles at the player's
// stage; confirm opens a rumble's INFORMATION, and confirm again enters it
// and leaves for its battle. Cancel closes the popup or moves to EXIT. The
// art is the schedule's; see data/ui/slai_rumble.json.

#include "phantomcrash/contest_info.hpp"
#include "phantomcrash/menu_cursor.hpp"
#include "phantomcrash/message_box.hpp"
#include "phantomcrash/schedule.hpp"
#include "phantomcrash/schedule_slai.hpp"
#include "phantomcrash/slai_rumble_rules.hpp"
#include "phantomcrash/town_screen.hpp"

#include <array>
#include <optional>
#include <string>
#include <vector>

namespace phantomcrash {

// A badge drawn over a whole venue tile, as the Phantom Crash schedule
// draws its class badges (none listed: every other regulation), and the
// class the INFORMATION popup names.
struct SlaiHubBadge {
  std::vector<int> regulations;
  TexelRect rect{};
  std::string class_text;
};

// A city's art on its S.L.A.I. Wired sheet: the emblem and the
// "<CITY> / DAY" and "<CITY> / NIGHT" labels.
struct SlaiCityArt {
  std::string image;
  TexelRect emblem{}, day{}, night{};
};

struct SlaiRumbleHubScript : TownBasics {
  Buttons up{}, down{};
  std::string close_sound, refuse_sound;
  Sprite exit, exit_selected;
  std::string cell_image;
  // Row r's top-left is origin + r x step; the rest is from there.
  std::array<float, 2> row_origin{}, row_step{};
  std::array<float, 2> tile_at{};
  ScheduleVenueTiles tiles;
  int tile_layer{8};
  int badge_layer{0x102};
  std::vector<SlaiHubBadge> badges;
  std::uint32_t badge_dim{0x808080};
  Sprite clear;
  SlaiSchedulePlate plate; // y from the row's top
  SlaiScheduleLabel city;  // in the plate's box
  SlaiScheduleLabel rule;  // the regulation's name, from the row's top-left
  // With the city's sheet installed, its emblem fitted in emblem_box (x on
  // the canvas, y from the row's top) and its DAY or NIGHT label at
  // art_label_at (from the row's top-left) scaled by art_label_scale, in
  // place of the plate and the city's name.
  std::vector<SlaiCityArt> city_art; // by the table's city column
  std::array<float, 4> emblem_box{};
  std::array<float, 2> art_label_at{};
  float art_label_scale{1.0F};
  int art_layer{0x106};
  // ENTRY and PRIZE: the word, then the amount in the HUD's money font.
  SlaiScheduleLabel entry_label, prize_label;
  std::string entry_text, prize_text;
  std::array<float, 2> entry_at{}, prize_at{};
  int amount_digits{7};
  std::uint32_t amount_colour{0xffffff};
  int amount_layer{0x104};
  // The cursor's frame and pulse cut into nine and stretched over the row.
  Sprite cursor_frame, cursor_pulse;
  float cursor_border{};
  std::array<float, 4> cursor_box{}; // row-relative
  ContestInfoStyle info;
  std::vector<std::string> city_names; // by the table's city column
  std::vector<std::string> closed;     // shown when no city is open
  // Shown instead when the day's rumbles are all ones exclude_records
  // leaves to another screen (the tutorial's, Battle School's); none: closed.
  std::vector<std::string> excluded;
  // A notice when entry is refused (0x259a40's order): the money held is
  // short, the ridden robot is not ready, its body is not the rumble's
  // maker, its legs not of the rumble's classes, or it breaks the rumble's
  // special rule; or a rental-only rumble has no robot to lend. {entry} is
  // the money the rumble needs, {rumble} its regulation's name and {maker}
  // the maker it takes.
  std::vector<std::string> short_of_money, not_ready, wrong_maker, wrong_legs, special_unmet,
      no_rental;
  std::string battle; // the screen a rumble entered leads to
  // Which of the day's rumbles the screen lists, by their save record: only
  // `records` when any are named, never `exclude_records`; with
  // `one_per_match`, a match the rota offers in several cities only once
  // (Battle School's tutorial).
  std::vector<int> records, exclude_records;
  bool one_per_match{};
  // Lists the rumbles of the tutorial's stage (0) while its record is not
  // cleared, whatever stage the ladder schedules (Battle School).
  bool tutorial{};
};

// Throws std::runtime_error naming the member at fault.
[[nodiscard]] SlaiRumbleHubScript parse_slai_rumble_hub(const json::JsonValue &document);

// The badge over a rumble's tile: the first listing its regulation, else
// the catch-all.
[[nodiscard]] const SlaiHubBadge *slai_hub_badge(const SlaiRumbleHubScript &script, int regulation);

class SlaiRumbleHub final {
public:
  // `back` is where EXIT leads.
  // `riding` is the ridden robot in S.L.A.I.'s terms (slai_entry_robot.hpp);
  // `rentals` that a rental-only rumble has robots to lend.
  SlaiRumbleHub(SlaiRumbleHubScript script, const SlaiRumbleData &data, const QuestSave &quest,
                SlaiRobot riding, bool rentals, std::string back, MessageBoxStyle box = {});

  void update(double tick_seconds, const InputFrame &input);

  [[nodiscard]] const SlaiRumbleHubScript &script() const noexcept {
    return script_;
  }
  [[nodiscard]] const SlaiRumbleData &data() const noexcept {
    return data_;
  }
  [[nodiscard]] const QuestSave &quest() const noexcept {
    return quest_;
  }
  // Today's open cities' rumbles, in display order.
  [[nodiscard]] const std::vector<SlaiCityMatch> &matches() const noexcept {
    return matches_;
  }
  // None listed, but some open city holds a rumble exclude_records leaves out.
  [[nodiscard]] bool only_excluded() const noexcept {
    return matches_.empty() && excluded_;
  }
  [[nodiscard]] int row() const noexcept {
    return row_;
  }
  [[nodiscard]] int exit_row() const noexcept {
    return static_cast<int>(matches_.size());
  }
  // The rumble under the cursor, or none on EXIT.
  [[nodiscard]] const SlaiCityMatch *selected() const;
  // The rumble entered, once the screen leaves for its battle.
  [[nodiscard]] std::optional<SlaiCityMatch> entered() const;
  // A row's box on the canvas, for the pointer.
  [[nodiscard]] Sprite row_box(int row) const;
  [[nodiscard]] const GrowingPopup &popup() const noexcept {
    return popup_;
  }
  [[nodiscard]] const MessageBox &box() const noexcept {
    return box_;
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
  void handle_input(double tick_seconds, InputFrame input);
  // Confirm in the open popup: enter, or say why not.
  void enter();
  // The script lists this rumble (records, exclude_records, one_per_match).
  [[nodiscard]] bool listed(const SlaiCityMatch &match) const;

  SlaiRumbleHubScript script_;
  const SlaiRumbleData &data_;
  QuestSave quest_;
  SlaiRobot riding_;
  bool rentals_{};
  std::string back_;
  std::vector<SlaiCityMatch> matches_;
  bool excluded_{};
  ScreenFade fade_;
  CursorRepeat rows_repeat_;
  GrowingPopup popup_;
  MessageBox box_;
  SoundQueue sounds_;
  int row_{};
  bool entering_{};
  double seconds_{};
};

} // namespace phantomcrash
