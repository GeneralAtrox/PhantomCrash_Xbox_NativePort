#include "desktop_battle_panel_text.hpp"
#include "desktop_text.hpp"

#include <algorithm>
#include <sstream>

namespace phantomcrash {

BattlePanelText::BattlePanelText(TextureCache &textures, const GlyphFont &font,
                                 const BattlePanelsStyle &style)
    : textures_(textures), font_(font) {
  const auto add = [&](const Sprite &source, const std::string &label) {
    if (!source.image.empty()) {
      replacements_[{source.image, source.rect}] = {source, label, {}, {}};
    }
  };
  for (std::size_t digit = 0; digit < 10; ++digit) {
    const auto label = std::to_string(digit);
    add(style.font.digits[digit], label);
    add(style.weapons.left.digits[digit], label);
    add(style.weapons.right.digits[digit], label);
  }
  add(style.font.comma, ",");
  add(style.colon, ":");
  add(style.weapons.left.percent, "%");
  add(style.weapons.right.percent, "%");
  for (const auto &replacement : style.text_replacements) {
    replacements_[{replacement.source.image, replacement.source.rect}] = replacement;
  }
}

void BattlePanelText::text(const Sprite &sprite, const std::string &label, const Viewport &viewport,
                           const float alpha) const {
  const auto size = sprite.size.value_or(
      std::array<float, 2>{sprite.rect[2] - sprite.rect[0], sprite.rect[3] - sprite.rect[1]});
  if (font_.cell <= 0 || size[0] <= 0 || size[1] <= 0) {
    return;
  }
  std::vector<std::vector<GlyphQuad>> lines;
  std::istringstream stream(label);
  std::string line;
  float width = 0;
  while (std::getline(stream, line)) {
    auto glyphs = layout_run(font_, glyph_run(font_, line), 0, 0);
    for (const auto &glyph : glyphs) {
      width = std::max(width, glyph.x + glyph.width);
    }
    lines.push_back(std::move(glyphs));
  }
  if (width <= 0 || lines.empty()) {
    return;
  }
  const float height = font_.cell * static_cast<float>(lines.size());
  const float scale = std::min(size[0] / width, size[1] / height);
  const float left = sprite.position[0] - (sprite.centered ? size[0] / 2 : 0);
  const float top = sprite.position[1] - (sprite.centered ? size[1] / 2 : 0);
  for (std::size_t i = 0; i < lines.size(); ++i) {
    auto &glyphs = lines[i];
    for (auto &glyph : glyphs) {
      glyph.x = left + glyph.x * scale;
      glyph.y = top + (size[1] - height * scale) / 2 +
                (glyph.y + static_cast<float>(i) * font_.cell) * scale;
      glyph.width *= scale;
      glyph.height *= scale;
    }
    draw_glyphs(textures_, font_, glyphs, viewport, sprite.colour, std::nullopt, alpha);
  }
}

bool BattlePanelText::draw(const Sprite &sprite, const Viewport &viewport,
                           const float alpha) const {
  const auto found = replacements_.find({sprite.image, sprite.rect});
  if (found == replacements_.end()) {
    return false;
  }
  const auto &replacement = found->second;
  Sprite label = sprite;
  if (replacement.region) {
    const auto &r = *replacement.region;
    const float width = sprite.rect[2] - sprite.rect[0];
    const float height = sprite.rect[3] - sprite.rect[1];
    if (r[0] < 0 || r[1] < 0 || r[2] > width || r[3] > height || r[2] <= r[0] || r[3] <= r[1]) {
      return false;
    }
    const auto size = sprite.size.value_or(std::array<float, 2>{width, height});
    const float sx = size[0] / width, sy = size[1] / height;
    const float x = sprite.position[0] - (sprite.centered ? size[0] / 2 : 0);
    const float y = sprite.position[1] - (sprite.centered ? size[1] / 2 : 0);
    // Keep the frame art outside the lettering; never paint over live world pixels.
    const std::array<std::array<float, 4>, 4> pieces{{{0, 0, width, r[1]},
                                                      {0, r[3], width, height},
                                                      {0, r[1], r[0], r[3]},
                                                      {r[2], r[1], width, r[3]}}};
    for (const auto &piece : pieces) {
      if (piece[2] <= piece[0] || piece[3] <= piece[1]) {
        continue;
      }
      Sprite frame = sprite;
      frame.centered = false;
      frame.rect = {sprite.rect[0] + piece[0], sprite.rect[1] + piece[1], sprite.rect[0] + piece[2],
                    sprite.rect[1] + piece[3]};
      frame.position = {x + piece[0] * sx, y + piece[1] * sy};
      frame.size = std::array<float, 2>{(piece[2] - piece[0]) * sx, (piece[3] - piece[1]) * sy};
      draw_sprite(textures_, frame, viewport, 0, alpha);
    }
    label.centered = false;
    label.position = {x + r[0] * sx, y + r[1] * sy};
    label.size = std::array<float, 2>{(r[2] - r[0]) * sx, (r[3] - r[1]) * sy};
  }
  label.colour = replacement.colour.value_or(sprite.colour);
  text(label, replacement.text, viewport, alpha);
  return true;
}

} // namespace phantomcrash
