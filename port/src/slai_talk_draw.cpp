// What a talk scene shows, as quads in the PS2 screen's 512x448 pixels, in
// the sprite engine's order: the town's hidden cells, the backdrop and
// rings, the talk objects, portraits (behind the windows, which hide their
// lower half), windows, the Knocker, then text and the fade.

#include "phantomcrash/slai_talk_scene.hpp"

#include <cmath>
#include <cstdio>

namespace phantomcrash {
namespace {

constexpr float screen_w = 512.0F, screen_h = 448.0F;
constexpr std::array<const char *, 4> common_pictures{
    "talk/common/tal01.png", "talk/common/tal02.png", "talk/common/tal03.png",
    "talk/common/tal04.png"};
const std::string freeze_picture = "talk/common/freeze.png";
// The town scenes' text bar: the port draws it in place of S.L.A.I.'s town
// HUD message box, which the port's town does not have.
constexpr float town_bar_top = 370.0F;
constexpr std::array<float, 4> town_bar_rgba{0.0F, 0.0F, 0.06F, 0.7F};

SlaiTalkQuad rect(float x0, float y0, float x1, float y1, std::array<float, 4> rgba,
                  const std::string &texture = {}, std::array<float, 4> uv = {}) {
  SlaiTalkQuad quad;
  quad.texture = texture;
  quad.corners = {{{x0, y0, uv[0], uv[1], rgba},
                   {x1, y0, uv[2], uv[1], rgba},
                   {x0, y1, uv[0], uv[3], rgba},
                   {x1, y1, uv[2], uv[3], rgba}}};
  return quad;
}

void push_sprite(std::vector<SlaiTalkQuad> &out, const SlaiTalkSprite &sprite) {
  if (!sprite.visible || sprite.texture.empty() || sprite.alpha <= 0.0F) {
    return;
  }
  auto rgba = gs_rgba(sprite.colour);
  rgba[3] = sprite.alpha / 128.0F;
  const float hw = sprite.w * 0.5F * sprite.scale_x;
  const float hh = sprite.h * 0.5F * sprite.scale_y;
  const float c = std::cos(sprite.rotation);
  const float s = std::sin(sprite.rotation);
  SlaiTalkQuad quad = rect(0, 0, 0, 0, rgba, sprite.texture,
                           {sprite.u, sprite.v, sprite.u + sprite.w, sprite.v + sprite.h});
  quad.blend = sprite.blend;
  const std::array<std::array<float, 2>, 4> corners{{{-hw, -hh}, {hw, -hh}, {-hw, hh}, {hw, hh}}};
  for (std::size_t i = 0; i < corners.size(); ++i) {
    quad.corners[i].x = sprite.x + corners[i][0] * c - corners[i][1] * s;
    quad.corners[i].y = sprite.y + corners[i][0] * s + corners[i][1] * c;
  }
  out.push_back(std::move(quad));
}

template <typename Texture>
void push_object(std::vector<SlaiTalkQuad> &out, const AnbObject &object, Texture texture) {
  for (auto &quad : anb_quads(object)) {
    SlaiTalkQuad talk;
    talk.texture = texture(quad.texture);
    talk.blend = quad.blend;
    talk.corners = quad.corners;
    if (!talk.texture.empty()) {
      out.push_back(std::move(talk));
    }
  }
}

} // namespace

void SlaiTalkScene::text_quads(const SlaiTalkText &text, std::vector<SlaiTalkQuad> &out) const {
  const float sx = text.w / 18.0F;
  const float sy = text.h / 18.0F;
  auto rgba = gs_rgba(text.colour);
  float x = text.x;
  float y = text.y;
  const std::size_t shown =
      text.shown < 0 ? text.glyphs.size() : std::min<std::size_t>(text.glyphs.size(), text.shown);
  for (std::size_t i = 0; i < shown; ++i) {
    const auto glyph = text.glyphs[i];
    if (glyph >= slai_talk_glyph::style_first && glyph <= slai_talk_glyph::style_last) {
      const auto index = static_cast<std::size_t>(glyph - slai_talk_glyph::style_first);
      rgba =
          gs_rgba(index < table_.style_colours.size() ? table_.style_colours[index] : text.colour);
      continue;
    }
    if (glyph == slai_talk_glyph::style_end) {
      rgba = gs_rgba(text.colour);
      continue;
    }
    if (glyph == slai_talk_glyph::newline) {
      x = text.x;
      y += text.h;
      continue;
    }
    if (glyph >= 0xFF00) {
      continue;
    }
    const int page = glyph / font_.per_page;
    const int cell = glyph % font_.per_page;
    if (page < static_cast<int>(font_.pages.size())) {
      const float u = static_cast<float>(cell % font_.columns * font_.cell_width);
      const float v = static_cast<float>(cell / font_.columns * font_.cell_height);
      const float left = glyph < font_.left.size() ? static_cast<float>(font_.left[glyph]) : 0.0F;
      const float x0 = x - left * sx;
      out.push_back(rect(x0, y, x0 + font_.cell_width * sx, y + font_.cell_height * sy, rgba,
                         "font/" + font_.pages[static_cast<std::size_t>(page)],
                         {u, v, u + font_.cell_width, v + font_.cell_height}));
    }
    x += glyph_advance(glyph, text.w);
  }
}

void SlaiTalkScene::login_quads(std::vector<SlaiTalkQuad> &out) const {
  // 0x26C070's FREEZE.PIG pieces and text.
  constexpr std::array<float, 4> fill{6.0F / 128, 6.0F / 128, 0x6E / 128.0F, 1.0F};
  out.push_back(rect(0, 0, screen_w, screen_h, fill, freeze_picture, {4, 123, 6, 125}));
  constexpr float logo_scale = 1.2F;
  out.push_back(rect(256 - 128 * logo_scale, 185 - 60 * logo_scale, 256 + 128 * logo_scale,
                     185 + 60 * logo_scale, {1, 1, 1, 1}, freeze_picture, {0, 0, 256, 120}));
  constexpr float bar_x = 317.0F, bar_y = 384.0F, bar_w = 170.0F * 2, bar_h = 4.0F * 4;
  const float left = bar_x - bar_w / 2;
  out.push_back(rect(left, bar_y - bar_h / 2, left + bar_w, bar_y + bar_h / 2, {0, 0, 0, 1},
                     freeze_picture, {3, 122, 173, 126}));
  const float filled = bar_w * static_cast<float>(login_percent_) / 100.0F;
  out.push_back(rect(left, bar_y - bar_h / 2, left + filled, bar_y + bar_h / 2, {1, 1, 0, 1},
                     freeze_picture, {3, 122, 3 + 170 * filled / bar_w, 126}));
  std::array<char, 8> digits{};
  std::snprintf(digits.data(), digits.size(), "%3d", login_percent_);
  SlaiTalkText number{slai_talk_glyphs(digits.data()), 42.0F, 375.0F, 16.0F, 17.0F, 0x807D8070U};
  text_quads(number, out);
  SlaiTalkText label{slai_talk_glyphs(table_.login_label),
                     42.0F + text_width(number),
                     375.0F,
                     16.0F,
                     17.0F,
                     0x807D8070U};
  text_quads(label, out);
  if (!table_.login_message.empty()) {
    SlaiTalkText message{
        slai_talk_glyphs(table_.login_message), 0, 401.0F, 18.0F, 18.0F, 0x807D8070U};
    message.x = 480.0F - text_width(message); // flag 4: right-aligned
    text_quads(message, out);
  }
}

std::vector<SlaiTalkQuad> SlaiTalkScene::draw() const {
  std::vector<SlaiTalkQuad> out;
  const float cell_w = screen_w / glitch_columns;
  const float cell_h = screen_h / glitch_rows;
  for (int i = 0; i < glitch_cells; ++i) {
    if (glitch_hidden_.test(static_cast<std::size_t>(i))) {
      // The edge columns reach past the PS2 screen to a wide window's edges.
      const int column = i % glitch_columns;
      const float x = static_cast<float>(column) * cell_w;
      const float y = static_cast<float>(i / glitch_columns) * cell_h;
      const float x0 = column == 0 ? -screen_w : x;
      const float x1 = column == glitch_columns - 1 ? 2 * screen_w : x + cell_w;
      out.push_back(rect(x0, y, x1, y + cell_h, {0, 0, 0, 1}));
    }
  }
  push_sprite(out, backdrop_);
  for (const auto &sprite : sprites_) {
    if (&sprite - sprites_.data() < slai_talk_stage::meters) {
      push_sprite(out, sprite);
    }
  }
  for (const auto &badge : badges_) {
    push_sprite(out, badge.face);
  }
  const auto common = [this](int index) -> std::string {
    if (index >= 0 && index < static_cast<int>(common_pictures.size())) {
      return common_pictures[static_cast<std::size_t>(index)];
    }
    return index == static_cast<int>(common_pictures.size()) ? icon_texture_ : std::string{};
  };
  for (const auto &object : objects_) {
    push_object(out, object, common);
  }
  push_sprite(out, picture_);
  for (const auto *portrait : {&previous_portrait_, &portrait_}) {
    const auto picture = [portrait](int) { return portrait->texture; };
    push_object(out, portrait->body, picture);
    push_object(out, portrait->eyes, picture);
    push_object(out, portrait->mouth, picture);
  }
  push_object(out, chip_, common);
  push_object(out, chip_window_, common);
  for (const auto &window : windows_) {
    push_object(out, window.frame, common);
    push_object(out, window.layer, common);
  }
  for (const auto &sprite : panel_icons_) {
    push_sprite(out, sprite);
  }
  for (int i = slai_talk_stage::meters; i < slai_talk_stage::sprite_count; ++i) {
    push_sprite(out, sprites_[static_cast<std::size_t>(i)]);
  }
  push_object(out, knocker_, [this](int) { return knocker_texture_; });
  const auto &main = windows_[0];
  if (mode_ == SlaiTalkMode::town && (!main.lines.empty() || main.name)) {
    out.push_back(rect(-screen_w, town_bar_top, 2 * screen_w, screen_h, town_bar_rgba));
  }
  for (const auto &badge : badges_) {
    text_quads(badge.name, out);
  }
  for (const auto &window : windows_) {
    if (window.name) {
      text_quads(*window.name, out);
    }
    for (const auto &line : window.lines) {
      text_quads(line, out);
    }
  }
  if (login_frame_ >= 0) {
    login_quads(out);
  }
  if (fade_ > 0.0F) {
    out.push_back(rect(0, 0, screen_w, screen_h, {0, 0, 0, fade_}));
  }
  return out;
}

} // namespace phantomcrash
