#include "desktop_draw_list.hpp"

#include "desktop_text.hpp"

#include <SDL3/SDL.h>

#include <algorithm>
#include <cmath>
#include <utility>

namespace phantomcrash {

void DrawList::sprite(const Sprite &source, const float alpha) {
  if (alpha <= 0.0F) {
    return;
  }
  commands_.push_back({source.layer, [this, copy = source, alpha] {
                         draw_sprite(textures_, copy, viewport_, seconds_, alpha);
                       }});
}

void DrawList::sprite(const Sprite &source, const std::array<float, 4> &clip, const float alpha) {
  if (alpha <= 0.0F) {
    return;
  }
  commands_.push_back({source.layer, [this, copy = source, clip, alpha] {
                         const auto [left, top, right, bottom] = clip;
                         const auto x0 = static_cast<int>(std::lround(viewport_.to_pixel_x(left)));
                         const auto y0 = static_cast<int>(std::lround(viewport_.to_pixel_y(top)));
                         const auto x1 = static_cast<int>(std::lround(viewport_.to_pixel_x(right)));
                         const auto y1 =
                             static_cast<int>(std::lround(viewport_.to_pixel_y(bottom)));
                         const SDL_Rect pixels{x0, y0, x1 - x0, y1 - y0};
                         SDL_SetRenderClipRect(textures_.renderer(), &pixels);
                         draw_sprite(textures_, copy, viewport_, seconds_, alpha);
                         SDL_SetRenderClipRect(textures_.renderer(), nullptr);
                       }});
}

void DrawList::text(const int layer, std::vector<GlyphQuad> glyphs, const std::uint32_t colour,
                    const std::optional<std::array<float, 4>> &clip, const float alpha) {
  if (alpha <= 0.0F) {
    return;
  }
  commands_.push_back({layer, [this, glyphs = std::move(glyphs), colour, clip, alpha] {
                         draw_glyphs(textures_, font_, glyphs, viewport_, colour, clip, alpha);
                       }});
}

void DrawList::ticker(const TickerStyle &style, const std::vector<int> &run, const double seconds) {
  text(style.layer, place_ticker(font_, run, style, seconds), style.colour, style.clip);
}

void DrawList::message_box(const MessageBox &box) {
  if (!box.is_open()) {
    return;
  }
  const MessageBoxStyle &style = box.style();
  Sprite window = style.window;
  const auto [scale_x, scale_y] = box.scale();
  window.size = std::array<float, 2>{(window.rect[2] - window.rect[0]) * scale_x,
                                     (window.rect[3] - window.rect[1]) * scale_y};
  sprite(window);
  if (!box.grown()) {
    return;
  }
  float y = style.text_position[1];
  for (const auto &line : box.lines()) {
    text(style.text_layer, layout_run(font_, glyph_run(font_, line), style.text_position[0], y),
         style.text_colour);
    y += style.line_height;
  }
  const auto choice = [&](const ChoiceSprites &sprites, const bool chosen) {
    sprite(sprites.normal);
    if (chosen) {
      sprite(sprites.selected);
    }
  };
  if (box.kind() == MessageBox::Kind::question) {
    choice(style.yes, box.choice() == 0);
    choice(style.no, box.choice() == 1);
  } else if (box.kind() == MessageBox::Kind::notice) {
    choice(style.ok, true);
  }
}

void DrawList::flush() {
  std::ranges::stable_sort(commands_, {}, &Command::layer);
  for (const auto &command : commands_) {
    command.draw();
  }
  commands_.clear();
}

void clear_black(TextureCache &textures) {
  SDL_Renderer *renderer = textures.renderer();
  SDL_SetRenderDrawColor(renderer, 0, 0, 0, 255);
  SDL_RenderClear(renderer);
}

void fill_black(TextureCache &textures, const float alpha) {
  if (alpha <= 0.0F) {
    return;
  }
  SDL_Renderer *renderer = textures.renderer();
  SDL_SetRenderDrawColor(renderer, 0, 0, 0, static_cast<Uint8>(alpha * 255.0F + 0.5F));
  SDL_RenderFillRect(renderer, nullptr);
}

} // namespace phantomcrash
