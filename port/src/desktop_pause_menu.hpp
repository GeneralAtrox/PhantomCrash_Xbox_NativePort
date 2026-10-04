#pragma once

#include "phantomcrash/glyph_font.hpp"
#include "phantomcrash/input.hpp"
#include "phantomcrash/json.hpp"

#include <array>
#include <cstdint>
#include <string>

namespace phantomcrash {

class DrawList;

// The battle's pause menu (a port addition, ui/battle_pause.json): RESUME,
// chosen when it opens, and EXIT beside it; EXIT asks NO / YES before the
// match is left. The buttons are the result screen's pill drawn as wide as
// their words.
class PauseMenu {
public:
  enum class Choice { none, resume, exit };

  // `extra_pause`: buttons that open it besides the file's.
  void load(const json::JsonValue &data, Buttons extra_pause);
  [[nodiscard]] Buttons pause_buttons() const noexcept {
    return pause_buttons_;
  }
  [[nodiscard]] float dim() const noexcept {
    return dim_;
  }
  void open() noexcept;
  // What the player chose this tick, while it is open.
  [[nodiscard]] Choice tick(const InputFrame &input);
  void draw(DrawList &list, const GlyphFont &font) const;

private:
  struct Line {
    std::array<float, 2> at{}; // the text's centre top
    std::uint32_t colour{0xffffff};
    std::array<std::string, 2> text; // in the menu, when asking
  };
  struct Pill {
    std::string image;
    std::array<float, 4> plain{}, lit{}; // the pill's rectangles, texels
    float cap{};                         // texels at each end kept whole
    float fill_column{};                 // a column of the plain middle, from the left
    float width{};                       // each button
    float gap{};                         // between them
    float y{};
    std::uint32_t text_colour{}, lit_text_colour{};
  };

  [[nodiscard]] std::array<float, 4> button_box(int index) const;
  [[nodiscard]] int button_at(const std::array<float, 2> &point) const;
  Choice choose(int index);
  void draw_button(DrawList &list, const GlyphFont &font, int index) const;

  Buttons pause_buttons_{}, back_buttons_{}, choose_buttons_{};
  float dim_{};
  int layer_{};
  Line title_, help_;
  Pill pill_;
  std::array<std::array<std::string, 2>, 2> labels_; // [asking][button]
  int cursor_{};
  bool asking_{};
};

} // namespace phantomcrash
