#pragma once

#include "phantomcrash/input.hpp"
#include "phantomcrash/json.hpp"
#include "phantomcrash/location_frame.hpp"
#include "phantomcrash/menu_cursor.hpp"
#include "phantomcrash/menu_screen.hpp"
#include "phantomcrash/settings.hpp"
#include "phantomcrash/sprite_layer.hpp"
#include "phantomcrash/town_map_layout.hpp"

#include <array>
#include <map>
#include <optional>
#include <string>
#include <utility>
#include <vector>

namespace phantomcrash {

// One place on the town map: its marker (a numbered disc at `spot`, view
// units, with its bar code and `name` to `side`, -1 left or 1 right, the
// name `label_width` wide), its sign, which grows out of `sign_from` (from
// the sign's top-left) when it is chosen, where the rider stands there and
// which way he faces, its help line and the screen it opens. A place with
// no `next` is closed: confirming it sounds "closed" and stays on the map.
// S.L.A.I.'s places (`slai`) are shown only with its content on, and a
// place can be left out with `enabled`.
struct TownPlace {
  std::string id;
  std::string next;
  HelpLine help;
  std::string name;
  std::string bars; // bar, space, bar... widths in modules, stretched across the name
  std::array<float, 2> spot{};
  int side{1};
  float label_width{};
  bool slai{};
  bool enabled{true};
  std::optional<Sprite> sign;
  std::array<float, 2> sign_from{};
  std::array<float, 2> rider{}; // the rider's top-left (canvas units)
  bool rider_flipped{};
  TownMarkerBoxes boxes; // where its marker is drawn and picked
};

// data/ui/town_map.json (original scene 41).
struct TownMapScript : MenuBasics {
  int scene{}; // the original's scene id; it seeds the music meter
  TickerStyle help;
  FramePlacement frame;
  Sprite rider;
  float bob{4.0F};                    // the rider hovers this far each way,
  double bob_x_seconds{100.0 / 60.0}; // across over this long
  double bob_y_seconds{64.0 / 60.0};  // and up and down over this
  double pop_seconds{8.0 / 60.0};     // a sign grows, the rider moves, appears or goes
  std::vector<TownPlace> places;
  TownView view;
  TownMarkerStyle markers;
  std::vector<TownBand> bands;
  std::vector<TownLettering> lettering;
  std::optional<TownWater> water;
  std::vector<std::array<float, 4>> keep_clear; // view units the markers stay out of
  Buttons back_buttons{};                       // the cursor steps back (to the lower number)
  Buttons forward_buttons{};                    // and forward,
  bool orient_to_start{};   // or, set as the map opens, forward first goes left on screen
  std::string place_sound;  // played with move_sound on a move
  std::string closed_sound; // played on confirming a closed place
  std::map<std::string, std::string> start_places; // screen before -> place
  int spin_pay{10};                                // NY for each full turn of the rider,
  int spin_limit{100};                             // until this much has been paid in the game
  std::string spin_sound;                          // played when a turn pays
};

// Throws std::runtime_error naming the entry at fault.
[[nodiscard]] TownMapScript parse_town_map(const json::JsonValue &document);

// The map with the places the content shows: S.L.A.I.'s only with `slai`,
// none that are not enabled.
[[nodiscard]] TownMapScript for_content(TownMapScript script, bool slai);

// The town map (0x3f329, 0x3eff2, drawn by 0x3edd2 and 0x3eadc). Once the
// map has faded in, the chosen place's sign grows and the rider appears at
// it; the cursor moves sideways round the places and wraps, the rider
// travelling to the new place; confirm fades out to its screen as the rider
// goes. There is no cancel. The cursor starts on the place the map was
// opened from.
//
// The mouse stands in for the original's right thumbstick: while it is in
// use the rider turns to face it, and each full turn round him pays
// `spin_pay` NY into the quest, up to `spin_limit` in a game. Moving it
// over a place's disc chooses the place, and clicking a place's disc or
// label (or the chosen place's sign) goes there.
class TownMap final {
public:
  TownMap(TownMapScript script, const std::string &from, QuestSave quest);

  void update(double tick_seconds, const InputFrame &input);

  [[nodiscard]] bool done() const noexcept {
    return fade_.done();
  }
  [[nodiscard]] const std::string &next() const {
    return fade_.next();
  }
  [[nodiscard]] std::vector<std::string> take_sounds() {
    return std::exchange(sounds_, {});
  }
  [[nodiscard]] const TownMapScript &script() const noexcept {
    return script_;
  }
  // The quest as the spins have left it (money and the NY paid).
  [[nodiscard]] const QuestSave &quest() const noexcept {
    return quest_;
  }
  [[nodiscard]] bool quest_changed() const noexcept {
    return quest_changed_;
  }
  [[nodiscard]] int place() const noexcept {
    return place_;
  }
  [[nodiscard]] bool moving() const noexcept;
  // +1 when forward counts up the places this visit, -1 when it counts down.
  [[nodiscard]] int forward_turn() const noexcept {
    return turn_;
  }
  // The chosen place's sign as it grows, or nothing before it starts.
  [[nodiscard]] std::optional<Sprite> sign() const;
  // The rider, or nothing while he is too small to see.
  [[nodiscard]] std::optional<Sprite> rider() const;
  [[nodiscard]] double seconds() const noexcept {
    return seconds_;
  }
  [[nodiscard]] double help_seconds() const noexcept {
    return help_seconds_;
  }
  [[nodiscard]] float black_alpha() const noexcept {
    return fade_.black_alpha();
  }

private:
  [[nodiscard]] float progress(std::optional<double> since) const;
  [[nodiscard]] std::optional<Sprite> unturned_rider() const;
  [[nodiscard]] std::optional<int> place_at(std::array<float, 2> point, bool names) const;
  void sound(const std::string &name);
  [[nodiscard]] int stepped_place(int step) const;
  void move_to(int next, bool wrapped);
  void follow_mouse(const InputFrame &input, int step);
  void spin(std::array<float, 2> pointer);
  void turn_to(double angle);

  TownMapScript script_;
  QuestSave quest_;
  bool quest_changed_{};
  ScreenFade fade_;
  CursorRepeat repeat_;
  int place_{};
  int previous_{}; // the place the rider travels from
  int turn_{1};    // the way round a forward step goes, +1 up the numbers
  bool wrapped_{}; // the last move went round past the ends
  double seconds_{};
  double help_seconds_{};
  std::optional<double> shown_;         // when the fade in ended
  std::optional<double> moved_;         // when the cursor last moved
  std::optional<double> bob_from_{0.0}; // the hover's clock; none while moving
  std::optional<double> leaving_;       // when confirm was pressed
  std::optional<int> hovered_;          // the place the mouse moved onto
  bool mouse_{};                        // the mouse is in use
  std::optional<double> facing_;        // the rider's turn to the mouse, radians
  double turned_{};                     // turns since the last payment, radians
  std::vector<std::string> sounds_;
};

} // namespace phantomcrash
