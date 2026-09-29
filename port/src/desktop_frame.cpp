#include "desktop_frame.hpp"

#include "desktop_draw_list.hpp"

#include <algorithm>
#include <utility>

namespace phantomcrash {

LocationFrame::LocationFrame(const GlyphFont &font, const FrameStyle &style,
                             const std::vector<MusicCredit> &credits, FramePlacement placement)
    : font_(font), style_(style), credits_(credits), placement_(std::move(placement)),
      meter_(style.meter) {}

void LocationFrame::set_music(const std::string &file) {
  if (file == file_) {
    return;
  }
  file_ = file;
  const std::string line =
      file.empty() ? std::string{} : credit_line(credits_, file, placement_.now_playing_format);
  credit_run_ = glyph_run(font_, line);
  credit_seconds_ = 0.0;
}

void LocationFrame::update(const double tick_seconds) {
  credit_seconds_ += tick_seconds;
  if (plate_seconds_ >= 0.0) {
    plate_seconds_ += tick_seconds;
  }
  meter_.update(tick_seconds, heard_);
}

FramePlacement credit_band(const MenuTickers &tickers) {
  FramePlacement placement;
  placement.hud = false;
  placement.meter = tickers.meter;
  placement.now_playing = tickers.now_playing;
  placement.now_playing_format = tickers.now_playing_format;
  return placement;
}

void LocationFrame::draw(DrawList &list, const QuestSave &quest) const {
  if (placement_.hud) {
    const HudStyle hud =
        placement_.hud_at ? placed_hud(style_.hud, *placement_.hud_at) : style_.hud;
    for (const auto &sprite : hud_sprites(hud, quest)) {
      list.sprite(sprite);
    }
  }
  // The plate shows only while a Scoobee is ridden (0x1d4a4).
  if (placement_.riding_plate && plate_seconds_ >= 0.0 && !quest.riding.empty()) {
    const RidingPlateStyle &style =
        placement_.riding_plate_style ? *placement_.riding_plate_style : style_.riding_plate;
    // The original's picture ends at x 0 and 640, so the sliding plate is
    // cut there rather than showing in the widescreen margin.
    Sprite plate = style.plate;
    plate.position[1] = placement_.riding_plate_y.value_or(plate.position[1]);
    const float x = plate.position[0] + riding_plate_offset(style, plate_seconds_);
    const float cut = std::max(-x, 0.0F);
    const float full = plate.rect[2] - plate.rect[0];
    const float width = full - cut - std::max(x + full - canvas_width, 0.0F);
    if (width > 0.0F) {
      plate.rect[0] += cut;
      plate.rect[2] = plate.rect[0] + width;
      plate.position[0] = x + cut;
      plate.size = std::array<float, 2>{width, plate.rect[3] - plate.rect[1]};
      list.sprite(plate);
    }
    std::vector<int> name = glyph_run(font_, quest.riding);
    name.resize(std::min(name.size(), static_cast<std::size_t>(style.max_glyphs)));
    list.text(
        style.name_layer,
        layout_run(font_, name, x + style.name_offset[0], plate.position[1] + style.name_offset[1]),
        style.name_colour, std::array<float, 4>{0.0F, 0.0F, canvas_width, canvas_height});
  }
  if (placement_.meter) {
    for (const auto &sprite : meter_sprites(style_.meter, *placement_.meter, meter_.levels())) {
      list.sprite(sprite);
    }
  }
  list.ticker(placement_.now_playing, credit_run_, credit_seconds_);
}

void LocationFrame::preload(TextureCache &textures) const {
  static_cast<void>(textures.get(font_.image));
  static_cast<void>(textures.get(style_.hud.day_label.image));
  static_cast<void>(textures.get(style_.meter.lit.image));
  if (placement_.riding_plate) {
    const RidingPlateStyle &plate =
        placement_.riding_plate_style ? *placement_.riding_plate_style : style_.riding_plate;
    static_cast<void>(textures.get(plate.plate.image));
  }
}

} // namespace phantomcrash
