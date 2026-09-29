#include "desktop_option.hpp"

#include "desktop_draw_list.hpp"

#include <utility>

namespace phantomcrash {
namespace {

FramePlacement option_frame(const OptionScript &script, const bool hud) {
  FramePlacement placement = credit_band(script);
  placement.hud = hud;
  placement.riding_plate = script.riding_plate_y.has_value();
  placement.riding_plate_y = script.riding_plate_y;
  return placement;
}

} // namespace

OptionScreen::OptionScreen(TextureCache &textures, const GlyphFont &font, OptionScript script,
                           MessageBoxStyle box, const GameOptions options, const FrameStyle &frame,
                           const std::vector<MusicCredit> &credits,
                           const std::string &returning_from, QuestSave quest, const bool hud)
    : textures_(textures), font_(font),
      menu_(std::move(script), std::move(box), options, returning_from), quest_(std::move(quest)),
      band_(font, frame, credits, option_frame(menu_.script(), hud)) {
  for (const auto &item : menu_.script().items) {
    help_runs_.push_back(glyph_run(font_, item.help.text));
  }
  // Load the textures now rather than on the first frame that needs them.
  static_cast<void>(textures_.get(font_.image));
  static_cast<void>(textures_.get(menu_.box().style().window.image));
  for (const auto &layer : menu_.script().layers) {
    static_cast<void>(textures_.get(layer.image));
  }
  band_.preload(textures_);
}

void OptionScreen::tick(const InputFrame &input, const double tick_seconds) {
  menu_.update(tick_seconds, input);
  if (menu_.faded_in()) {
    band_.start_riding_plate();
  }
  band_.update(tick_seconds);
}

void OptionScreen::draw(const Viewport &viewport) {
  clear_black(textures_);
  const OptionScript &script = menu_.script();
  DrawList list(textures_, font_, viewport, menu_.seconds());
  for (const auto &layer : script.layers) {
    list.sprite(layer);
  }
  for (std::size_t index = 0; index < script.items.size(); ++index) {
    const auto &item = script.items[index];
    list.sprite(item.normal);
    if (static_cast<int>(index) == menu_.cursor()) {
      // The selected item holds still while a box is open or a fade runs.
      Sprite selected = item.selected;
      if (!menu_.cursor_pulses()) {
        selected.animation = Animation::none;
      }
      list.sprite(selected);
    }
  }
  const auto cursor = static_cast<std::size_t>(menu_.cursor());
  list.ticker(help_style(script.help, script.items[cursor].help), help_runs_[cursor],
              menu_.help_seconds());
  band_.draw(list, quest_);
  list.message_box(menu_.box());
  list.flush();
  fill_black(textures_, menu_.black_alpha());
}

} // namespace phantomcrash
