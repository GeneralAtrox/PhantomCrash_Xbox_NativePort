// The talk VM's windows and text: open/close, styles, say and the button
// prompt (0x26CB50..0x26FFF0, text objects 0x207400).

#include "phantomcrash/slai_talk_scene.hpp"

#include <algorithm>
#include <stdexcept>
#include <string>

namespace phantomcrash {
namespace {

constexpr int glyph_unit = 18; // glyph sizes are given against an 18 pixel cell
constexpr int line_gap = 3;
constexpr float talk_left = 32.0F, talk_name_y = 298.0F, talk_text_y = 335.0F;
// 0x26FFF0: the town HUD in state 1 keeps the text low; otherwise it sits
// higher, above the HUD.
constexpr float town_left = 17.0F, town_text_y_low = 409.0F, town_text_y = 379.0F;
constexpr float window_text_dx = -0x55, window_text_dy = -0x19;
constexpr float window_name_dx = -0xA3, window_name_dy = -0x3F;
constexpr float window_prompt_dy = -135.0F;
constexpr std::uint32_t town_text_colour = 0x807D8070U;
constexpr int blip_cue = 0x0A;
constexpr int blip_frames = 3;
// Nameplate animations on VM+0x9C by style (0x26FC60); style 1 also has
// 0x19, chosen by flags not yet decoded.
constexpr std::array<int, 4> nameplate_anims{-1, 0x1A, -1, 0x2B};

std::uint32_t pick(const std::vector<std::uint32_t> &colours, int index) {
  return index >= 0 && static_cast<std::size_t>(index) < colours.size()
             ? colours[static_cast<std::size_t>(index)]
             : gs_white;
}

} // namespace

std::vector<std::uint16_t> SlaiTalkScene::expand(const std::vector<std::uint16_t> &glyphs) const {
  std::vector<std::uint16_t> out;
  const auto append = [&out](const std::string &text) {
    try {
      const auto units = slai_talk_glyphs(text);
      out.insert(out.end(), units.begin(), units.end());
    } catch (const std::runtime_error &) {
    }
  };
  for (const auto glyph : glyphs) {
    if (glyph == slai_talk_glyph::player) {
      append(names_.player);
    } else if (glyph == slai_talk_glyph::partner) {
      append(names_.partner);
    } else if (glyph == slai_talk_glyph::alias) {
      append(names_.alias);
    } else if (glyph == slai_talk_glyph::day) {
      append(std::to_string(names_.day));
    } else {
      out.push_back(glyph);
    }
  }
  return out;
}

float SlaiTalkScene::text_width(const SlaiTalkText &text) const {
  float width = 0.0F;
  for (const auto glyph : text.glyphs) {
    if (glyph < 0xFF00) {
      width += glyph_advance(glyph, text.w);
    }
  }
  return width;
}

// 0x208620: the advance in 1/16 pixels, rounded to whole pixels.
float SlaiTalkScene::glyph_advance(std::uint16_t glyph, float size) const {
  const int scale = (static_cast<int>(size) * 256 + 9) / glyph_unit;
  const int sixteenths = (scale * font_.advance(glyph)) >> 4;
  return static_cast<float>((sixteenths + 8) & ~0xF) / 16.0F;
}

void SlaiTalkScene::clear_window_text(int window) {
  auto &target = windows_[static_cast<std::size_t>(window < 0 ? window_ : window)];
  target.lines.clear();
  target.name.reset();
}

void SlaiTalkScene::reset_cursor(SlaiTalkWindow &window, int index) const {
  if (mode_ == SlaiTalkMode::town) {
    window.cursor_x = town_left;
  } else {
    window.cursor_x = index == 0 ? talk_left : window.x + window_text_dx;
  }
}

// 0x26CDD0: the style a window opens in, from the next case or style op.
int SlaiTalkScene::look_ahead_style() const {
  for (auto at = static_cast<std::size_t>(pc_); at < script_.code.size(); ++at) {
    const auto &next = script_.code[at];
    if (next.op == slai_talk_op::end) {
      return 0;
    }
    if (next.op == slai_talk_op::case_ && !next.args.empty() && next.args[0] == selector_) {
      return selector_ == 0 ? 2 : 3;
    }
    if (next.op == slai_talk_op::window_style && !next.args.empty()) {
      return next.args[0];
    }
  }
  return 0;
}

// 0x26FC60 ('+' scenes) and 0x26FFF0 (town scenes).
void SlaiTalkScene::apply_style(int style, int name_string) {
  auto &window = windows_[static_cast<std::size_t>(window_)];
  window.style = style;
  text_flags_ &= ~0x40U;
  clear_window_text(window_);
  window.glyph_w = window.glyph_h = 18.0F;
  window.ruby_w = window.ruby_h = 9.0F;
  if (mode_ == SlaiTalkMode::town) {
    window.cursor_x = town_left;
    window.cursor_y = town_hud_ == 1 ? town_text_y_low : town_text_y;
    window.text_colour = town_text_colour;
    return;
  }
  talk_sounds_ = 1;
  const auto index = static_cast<std::size_t>(std::clamp(style, 0, 3));
  const auto colours = index < table_.style_text_colours.size()
                           ? table_.style_text_colours[index]
                           : std::array<std::uint32_t, 2>{gs_white, gs_white};
  window.text_colour = colours[0];
  reset_cursor(window, window_);
  window.cursor_y = window_ == 0 ? talk_text_y : window.y + window_text_dy;
  if (name_string >= 0 && static_cast<std::size_t>(name_string) < script_.strings.size()) {
    SlaiTalkText name;
    name.glyphs = expand(script_.strings[static_cast<std::size_t>(name_string)]);
    name.x = window_ == 0 ? talk_left : window.x + window_name_dx;
    name.y = window_ == 0 ? talk_name_y : window.y + window_name_dy;
    name.colour = colours[1];
    window.name = std::move(name);
  }
  if (window_ == 0) {
    window.colour = pick(table_.window_colours, style);
    window.frame.colour = window.colour;
  }
  auto &plate = objects_[slai_talk_stage::nameplate];
  const int anim = nameplate_anims[index];
  if (anim < 0) {
    plate.flags |= anb_flag::hidden;
  } else if (plate.anim != anim || plate.has(anb_flag::hidden)) {
    plate.play(anim, anb_flag::hold, 0x81);
  }
}

bool SlaiTalkScene::exec_text(int op, const std::vector<int> &args) {
  const auto arg = [&args](std::size_t i) { return i < args.size() ? args[i] : 0; };
  auto &window = windows_[static_cast<std::size_t>(window_)];
  switch (op) {
  case slai_talk_op::open_window: { // 0x26CB50
    const int index = std::clamp(arg(0), 0, static_cast<int>(windows_.size()) - 1);
    auto &target = windows_[static_cast<std::size_t>(index)];
    target.character = arg(1);
    target.x = static_cast<float>(arg(2) + 256);
    target.y = static_cast<float>(arg(3) + 224);
    if (index == 0) {
      target.frame.x = 256.0F;
      target.frame.y = 224.0F;
      target.frame.play(4);
      target.colour = pick(table_.window_colours, look_ahead_style());
    } else {
      icon_texture_ = character_texture(arg(1));
      target.frame.x = target.layer.x = target.x;
      target.frame.y = target.layer.y = target.y;
      target.frame.play(0x17);
      target.layer.play(0x2C);
      target.colour = pick(table_.window_colours, arg(1) < 0x3A ? 0 : 1);
    }
    target.frame.colour = target.colour;
    target.open = true;
    target.speed = interval_;
    window_ = index;
    return true;
  }
  case slai_talk_op::close_window: { // 0x26CA20
    const int index = std::clamp(arg(0), 0, static_cast<int>(windows_.size()) - 1);
    auto &target = windows_[static_cast<std::size_t>(index)];
    target.frame.play(index == 0 ? 5 : 0x18, 0, 0x87);
    if (index != 0) {
      target.layer.play(0x2D, 0, 0x87);
    }
    target.open = false;
    clear_window_text(index);
    objects_[slai_talk_stage::nameplate].flags |= anb_flag::hidden;
    if (window_ == index) {
      window_ = 0;
    }
    closing_ = index;
    return true;
  }
  case slai_talk_op::select_window:
    window_ = std::clamp(arg(0), 0, static_cast<int>(windows_.size()) - 1);
    return false;
  case slai_talk_op::window_style: // 0x26FAE0
    apply_style(arg(0), arg(1));
    return false;
  case slai_talk_op::case_: // 0x26FB60
    case_value_ = arg(0);
    if (arg(0) == selector_) {
      apply_style(selector_ == 0 ? 2 : 3, arg(1));
    }
    return false;
  case slai_talk_op::glyph_size:
    window.glyph_w = static_cast<float>(arg(0));
    window.glyph_h = static_cast<float>(arg(1));
    return false;
  case slai_talk_op::ruby_size:
    window.ruby_w = static_cast<float>(arg(0));
    window.ruby_h = static_cast<float>(arg(1));
    return false;
  case slai_talk_op::text_speed:
    window.speed = arg(0);
    return false;
  case slai_talk_op::text_cursor:
    window.cursor_x = static_cast<float>(arg(0));
    window.cursor_y = static_cast<float>(arg(1));
    return false;
  case slai_talk_op::text_offset:
    window.cursor_x += static_cast<float>(arg(0));
    window.cursor_y += static_cast<float>(arg(1));
    return false;
  case slai_talk_op::clear_text:
    clear_window_text(arg(0));
    return false;
  case slai_talk_op::say:
    return say(arg(0));
  case slai_talk_op::wait_button: { // 0x26E740
    auto &prompt = objects_[slai_talk_stage::prompt];
    prompt.x = window_ == 0 || mode_ == SlaiTalkMode::town ? 256.0F : window.x;
    prompt.y = window_ == 0 || mode_ == SlaiTalkMode::town ? 224.0F : window.y + window_prompt_dy;
    prompt.play(10);
    prompt.colour = pick(table_.prompt_colours, window.style);
    return true;
  }
  default:
    return false; // ruby, badges and partner paths are not used by the opening
  }
}

// 0x26E150: a line of text at the cursor, typed when the window has a speed.
bool SlaiTalkScene::say(int string) {
  auto &window = windows_[static_cast<std::size_t>(window_)];
  if (string < 0 || static_cast<std::size_t>(string) >= script_.strings.size()) {
    return false;
  }
  SlaiTalkText text;
  text.glyphs = expand(script_.strings[static_cast<std::size_t>(string)]);
  text.x = window.cursor_x;
  text.y = window.cursor_y;
  text.w = window.glyph_w;
  text.h = window.glyph_h;
  text.colour = window.text_colour;
  const bool typed = window.speed != 0 && (text_flags_ & 0x40U) == 0;
  text.shown = typed ? 0 : -1;
  if ((text_flags_ & 4U) != 0) {
    window.cursor_x += text_width(text);
  } else {
    reset_cursor(window, window_);
    window.cursor_y += text.h + window.ruby_h + line_gap;
  }
  window.lines.push_back(std::move(text));
  return window.speed != 0;
}

bool SlaiTalkScene::poll_text(int op) {
  auto &window = windows_[static_cast<std::size_t>(window_)];
  const bool pressed = input_.advance || input_.hurry;
  switch (op) {
  case slai_talk_op::open_window:
    for (const auto &target : windows_) {
      if (target.open && !target.frame.has(anb_flag::ended)) {
        return false;
      }
    }
    return true;
  case slai_talk_op::close_window: {
    if (closing_ < 0) {
      return true;
    }
    const auto &target = windows_[static_cast<std::size_t>(closing_)];
    return target.frame.has(anb_flag::hidden);
  }
  case slai_talk_op::wait_button:
    if (!pressed) {
      return false;
    }
    input_.advance = false;
    objects_[slai_talk_stage::prompt].flags |= anb_flag::hidden;
    return true;
  case slai_talk_op::say: { // 0x26E380
    if ((text_flags_ & 2U) == 0 && pressed) {
      input_.advance = false;
      text_flags_ |= 0x40U;
      for (auto &line : window.lines) {
        line.shown = -1;
      }
      return true;
    }
    window.accumulator += window.speed;
    const int glyphs = window.accumulator / interval_;
    if (glyphs == 0) {
      return false;
    }
    if (blip_wait_ == 0 && talk_sounds_ != 0) {
      sound(blip_cue);
      blip_wait_ = blip_frames;
    }
    window.accumulator %= interval_;
    for (auto &line : window.lines) {
      if (line.shown >= 0 && line.shown < static_cast<int>(line.glyphs.size())) {
        line.shown += glyphs;
        if (line.shown >= static_cast<int>(line.glyphs.size())) {
          line.shown = -1;
        }
        break;
      }
      if (line.shown >= 0) {
        line.shown = -1;
      }
    }
    return std::all_of(window.lines.begin(), window.lines.end(),
                       [](const SlaiTalkText &line) { return line.shown < 0; });
  }
  default:
    return true;
  }
}

} // namespace phantomcrash
