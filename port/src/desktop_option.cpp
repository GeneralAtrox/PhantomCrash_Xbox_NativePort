#include "desktop_option.hpp"

#include "desktop_draw_list.hpp"

#include <utility>

namespace phantomcrash {

OptionScreen::OptionScreen(TextureCache &textures, const GlyphFont &font, OptionScript script,
                           MessageBoxStyle box, const GameOptions options,
                           const std::string &now_playing, const std::string &returning_from)
    : textures_(textures), font_(font),
      menu_(std::move(script), std::move(box), options, returning_from) {
  for (const auto &item : menu_.script().items) {
    help_runs_.push_back(glyph_run(font_, item.help.text));
  }
  now_playing_run_ = glyph_run(font_, now_playing);
  // Load the textures now rather than on the first frame that needs them.
  static_cast<void>(textures_.get(font_.image));
  static_cast<void>(textures_.get(menu_.box().style().window.image));
  for (const auto &layer : menu_.script().layers) {
    static_cast<void>(textures_.get(layer.image));
  }
}

void OptionScreen::tick(const InputFrame &input, const double tick_seconds) {
  menu_.update(tick_seconds, input);
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
  list.ticker(script.now_playing, now_playing_run_, menu_.seconds());
  list.message_box(menu_.box());
  list.flush();
  fill_black(textures_, menu_.black_alpha());
}

} // namespace phantomcrash
