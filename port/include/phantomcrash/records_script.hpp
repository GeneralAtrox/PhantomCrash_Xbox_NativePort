#pragma once

// data/ui/records.json: RECORDS (the original's scene 29), its menu, the
// two record pages, CONVENTION RECORD and how a page's rows appear.

#include "phantomcrash/input.hpp"
#include "phantomcrash/json.hpp"
#include "phantomcrash/sprite_layer.hpp"
#include "phantomcrash/town_screen.hpp"

#include <array>
#include <cstdint>
#include <map>
#include <optional>
#include <string>
#include <vector>

namespace phantomcrash {

enum class RecordFormat { count, percent, money, time };

// One row of a record page: a count from the save, a ratio of two counts
// (as a percentage), or the game's achievement rate.
struct RecordRow {
  std::string label;
  std::string stat;     // a QuestSave::records name, or "days_passed"
  std::string ratio_of; // the stat is divided by this one, times 100
  bool completion{};
  RecordFormat format{RecordFormat::count};
};

struct RecordPage {
  Sprite panel;
  PopupGrowth growth{0.3, 0.0F};
  std::vector<Sprite> hint;
  float label_x{};
  float first_y{};
  float step_y{};
  std::vector<RecordRow> rows;
};

// How a page's rows appear one after another (0x33b2c).
struct RowReveal {
  double first_seconds{0.3};
  double step_seconds{0.2};
  double fade_seconds{1.5};
  double whiten_seconds{5.0};
  std::uint32_t start_colour{0xffd300};
  Sprite bar;
  float bar_x{76.0F};
  float bar_above{7.0F};
  float bar_until{64.0F / 255.0F};       // of the fade, while the bar shows
  float bar_shrink_from{40.0F / 255.0F}; // of the fade, when it narrows
};

// Where the values go (0x331c0, 0x32f76).
struct RecordNumbers {
  float right{522.0F};
  float time_left{422.0F};
  int hours_max{99};
  std::string percent{"%"};
  std::string money{" NY"};
};

struct RecordsMenuItem {
  Sprite label;
  std::string opens; // a page, "convention" or "exit"
};

struct RecordsMenu {
  Sprite panel;
  std::vector<RecordsMenuItem> items;
  std::uint32_t lit_colour{0xffd300};
  int lit_layer{0x104};
  float behind_alpha{0.25F};
  int behind_panel_layer{2}; // under a page's panel (0x33aed)
  int behind_items_layer{3};
};

// A venue, or REGULATION: its contests in grid order, and per contest the
// champion team's name.
struct ConventionVenue {
  std::vector<int> contests;
  Sprite header;
  std::vector<Sprite> ranks; // the class columns' names
  std::vector<Sprite> champion_names;
};

struct ConventionRegulation {
  ConventionVenue contests;
  int columns{6};
  int rows{2};
  Sprite grid;
  int own_portrait_slot{-1}; // this slot's first portrait is the player's own
  Sprite own_portrait;
  std::vector<Sprite> arrows, arrows_lit;
  double flip_seconds{0.15};
};

struct HiddenChampion {
  int contest{no_hidden};
  int until_event{};
  std::vector<Sprite> portraits;
  Sprite name;
  static constexpr int no_hidden = -2;
};

struct ConventionStyle {
  Sprite ellipse;
  PopupGrowth ellipse_growth{0.5, 0.0F};
  Sprite first_ranker;
  int first_ranker_contest{};
  std::vector<std::array<float, 2>> first_ranker_portraits;
  std::vector<Sprite> items;
  std::vector<std::string> item_opens; // "venue", "regulation" or "back"
  std::uint32_t dim_colour{0x404040};
  double slide_seconds{0.5};
  float slide_distance{320.0F};
  float slide_stagger{40.0F};
  Sprite panel;
  PopupGrowth growth{0.3, 0.0F};
  std::vector<Sprite> hint;
  Sprite grid;
  Sprite cursor;
  std::array<float, 2> cursor_origin{};
  float cursor_step{62.0F};
  Sprite area_cursor;
  std::vector<Sprite> day_night;
  std::vector<std::array<float, 2>> portraits;
  int portrait_layer{0x107};
  Sprite loser;
  float loser_step{70.0F};
  std::array<float, 2> contest_name{};
  std::uint32_t text_colour{0xffffff};
  int text_layer{0x109};
  HiddenChampion hidden;
  std::vector<ConventionVenue> venues;
  ConventionRegulation regulation;
};

struct RecordsScript : TownBasics {
  Buttons up{}, down{}, left{}, right{}, page_back{}, page_forward{};
  std::string column_sound, exit_sound, close_sound, row_sound, page_sound;
  RecordsMenu menu;
  RowReveal reveal;
  RecordNumbers numbers;
  std::map<std::string, RecordPage> pages;
  std::vector<int> completion_contests;
  ConventionStyle convention;
};

// Throws std::runtime_error naming the member at fault.
[[nodiscard]] RecordsScript parse_records(const json::JsonValue &document);

} // namespace phantomcrash
