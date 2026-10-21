#include "desktop_town_map.hpp"

#include "desktop_draw_list.hpp"
#include "desktop_town_map_markers.hpp"

#include <algorithm>
#include <array>
#include <cstdint>
#include <utility>

namespace phantomcrash {
namespace {

// The view's canvas rectangle fitted, centred, into the window's screen.
Viewport view_viewport(const Viewport &window, const TownView &view) {
  Viewport fitted = window;
  const float width = view.rect[2] - view.rect[0];
  const float height = view.rect[3] - view.rect[1];
  const auto pixels_x = static_cast<float>(window.pixel_width);
  const auto pixels_y = static_cast<float>(window.pixel_height);
  fitted.scale = std::min(pixels_x / width, pixels_y / height);
  fitted.offset_x =
      window.origin_x + (pixels_x - width * fitted.scale) * 0.5F - view.rect[0] * fitted.scale;
  fitted.offset_y =
      window.origin_y + (pixels_y - height * fitted.scale) * 0.5F - view.rect[1] * fitted.scale;
  return fitted;
}

// Blacks out the window beyond the view, where the bands' outlines reach
// past its edges (a window narrower than the view's 16:9).
void black_beyond(TextureCache &textures, const Viewport &window, const Viewport &view,
                  const TownView &town) {
  const float left = view.to_pixel_x(town.rect[0]);
  const float top = view.to_pixel_y(town.rect[1]);
  const float right = view.to_pixel_x(town.rect[2]);
  const float bottom = view.to_pixel_y(town.rect[3]);
  const float x = window.origin_x;
  const float y = window.origin_y;
  const auto width = static_cast<float>(window.pixel_width);
  const auto height = static_cast<float>(window.pixel_height);
  const std::array<SDL_FRect, 4> bars{{{x, y, width, top - y},
                                       {x, bottom, width, y + height - bottom},
                                       {x, top, left - x, bottom - top},
                                       {right, top, x + width - right, bottom - top}}};
  SDL_Renderer *renderer = textures.renderer();
  SDL_SetRenderDrawColor(renderer, 0, 0, 0, 255);
  for (const SDL_FRect &bar : bars) {
    if (bar.w > 0.0F && bar.h > 0.0F) {
      SDL_RenderFillRect(renderer, &bar);
    }
  }
}

} // namespace

TownMapScreen::TownMapScreen(TextureCache &textures, const GlyphFont &font, TownMapScript script,
                             const std::string &from, const FrameStyle &frame,
                             const std::vector<MusicCredit> &credits, QuestSave quest)
    : textures_(textures), font_(font), map_(std::move(script), from, std::move(quest)),
      frame_(font, frame, credits, map_.script().frame) {
  const TownMapScript &data = map_.script();
  for (const auto &place : data.places) {
    help_runs_.push_back(glyph_run(font_, place.help.text));
  }
  for (const auto &layer : data.layers) {
    static_cast<void>(textures_.get(layer.image));
  }
  static_cast<void>(textures_.get(data.rider.image));
  frame_.preload(textures_);
  SDL_Log("Town map: from=%s place=%d back=0x%X forward=0x%X forward_counts=%s", from.c_str(),
          map_.place() + 1, data.back_buttons, data.forward_buttons,
          map_.forward_turn() > 0 ? "up" : "down");
}

InputFrame TownMapScreen::in_view(InputFrame input) const {
  if (input.pointer && window_ && view_) {
    const auto [x, y] = *input.pointer;
    input.pointer = std::array<float, 2>{(window_->to_pixel_x(x) - view_->offset_x) / view_->scale,
                                         (window_->to_pixel_y(y) - view_->offset_y) / view_->scale};
  }
  return input;
}

void TownMapScreen::tick(const InputFrame &window_input, const double tick_seconds) {
  const InputFrame input = in_view(window_input);
  const int before = map_.place();
  map_.update(tick_seconds, input);
  if (before != map_.place() || (input.pressed & button::directions) != 0) {
    SDL_Log("Town map input: left=%d right=%d up=%d down=%d held=0x%X mouse_moved=%d place=%d->%d",
            (input.pressed & button::left) != 0, (input.pressed & button::right) != 0,
            (input.pressed & button::up) != 0, (input.pressed & button::down) != 0, input.held,
            input.pointer_moved, before + 1, map_.place() + 1);
  }
  // The frame's RIDING SV plate starts once the map has faded in (0x3f2d7).
  if (map_.black_alpha() <= 0.0F) {
    frame_.start_riding_plate();
  }
  frame_.update(tick_seconds);
}

void TownMapScreen::draw(const Viewport &window) {
  clear_black(textures_);
  const TownMapScript &script = map_.script();
  window_ = window;
  view_ = view_viewport(window, script.view);
  const Viewport &viewport = *view_;
  DrawList list(textures_, font_, viewport, map_.seconds());
  for (const auto &layer : script.layers) {
    list.sprite(layer);
  }
  const auto chosen = static_cast<std::size_t>(map_.place());
  const TownPlace &place = script.places[chosen];
  draw_town_bands(list, script);
  draw_town_lettering(list, script);
  draw_town_markers(list, script, map_.place(), animation_alpha(Animation::pulse, map_.seconds()));
  if (const auto sign = map_.sign()) {
    list.sprite(*sign);
  }
  if (const auto rider = map_.rider()) {
    list.sprite(*rider);
  }
  list.ticker(help_style(script.help, place.help), help_runs_[chosen], map_.help_seconds());
  frame_.draw(list, map_.quest());
  list.flush();
  black_beyond(textures_, window, viewport, script.view);
  fill_black(textures_, map_.black_alpha());
}

} // namespace phantomcrash
