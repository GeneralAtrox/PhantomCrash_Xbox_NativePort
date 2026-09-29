#include "desktop_town_map.hpp"

#include "desktop_draw_list.hpp"

#include <cstdint>
#include <utility>

namespace phantomcrash {

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
}

void TownMapScreen::tick(const InputFrame &input, const double tick_seconds) {
  map_.update(tick_seconds, input);
  // The frame's RIDING SV plate starts once the map has faded in (0x3f2d7).
  if (map_.black_alpha() <= 0.0F) {
    frame_.start_riding_plate();
  }
  frame_.update(tick_seconds);
}

void TownMapScreen::draw(const Viewport &viewport) {
  clear_black(textures_);
  const TownMapScript &script = map_.script();
  DrawList list(textures_, font_, viewport, map_.seconds());
  for (const auto &layer : script.layers) {
    list.sprite(layer);
  }
  const auto chosen = static_cast<std::size_t>(map_.place());
  const TownPlace &place = script.places[chosen];
  list.sprite(place.badge);
  list.sprite(place.arrows);
  if (const auto sign = map_.sign()) {
    list.sprite(*sign);
  }
  if (const auto rider = map_.rider()) {
    list.sprite(*rider);
  }
  list.ticker(help_style(script.help, place.help), help_runs_[chosen], map_.help_seconds());
  frame_.draw(list, map_.quest());
  list.flush();
  fill_black(textures_, map_.black_alpha());
}

} // namespace phantomcrash
