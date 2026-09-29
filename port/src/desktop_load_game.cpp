#include "desktop_load_game.hpp"

#include "desktop_draw_list.hpp"

#include <algorithm>
#include <utility>

namespace phantomcrash {

LoadGameScreen::LoadGameScreen(TextureCache &textures, const GlyphFont &font, LoadGameScript script,
                               std::vector<SaveSlot> slots, const MessageBoxStyle &box,
                               const FrameStyle &frame, const std::vector<MusicCredit> &credits,
                               QuestSave quest, const bool hud, SaveLoadHooks hooks,
                               const bool unreadable)
    : textures_(textures), font_(font), style_(frame),
      menu_(std::move(script), std::move(slots), box, std::move(hooks), unreadable),
      quest_(std::move(quest)), frame_(font, frame, credits, with_hud(menu_.script().frame, hud)) {
  const LoadGameScript &data = menu_.script();
  help_run_ = glyph_run(font_, data.help_text);
  for (const auto &layer : data.layers) {
    static_cast<void>(textures_.get(layer.image));
  }
  static_cast<void>(textures_.get(data.saved_time.font.comma.image));
  static_cast<void>(textures_.get(box.window.image));
  frame_.preload(textures_);
}

void LoadGameScreen::tick(const InputFrame &input, const double tick_seconds) {
  menu_.update(tick_seconds, input);
  frame_.update(tick_seconds);
}

void LoadGameScreen::draw_details(DrawList &list, const QuestSave &quest) const {
  const SaveDetailsStyle &style = menu_.script().details;
  const auto text = [&](const std::string &value, const std::array<float, 2> &position) {
    std::vector<int> run = glyph_run(font_, value);
    run.resize(std::min(run.size(), static_cast<std::size_t>(style.max_glyphs)));
    list.text(style.text_layer, layout_run(font_, run, position[0], position[1]),
              style.text_colour);
  };
  text(quest.player_name, style.name);
  // The Scoobee's name shows only while one is ridden (slot 0-9, 0x6b65a).
  if (!quest.riding.empty()) {
    text(quest.riding, style.riding);
  }
  Sprite label = style.day_label;
  label.layer = style.number_layer;
  list.sprite(label);
  for (const auto &sprite :
       number_sprites(style_.hud.font, style.day, quest.day + 1, style.number_layer)) {
    list.sprite(sprite);
  }
  for (const auto &sprite : details_money_sprites(style_.hud.font, style, quest.money)) {
    list.sprite(sprite);
  }
}

void LoadGameScreen::draw(const Viewport &viewport) {
  clear_black(textures_);
  const LoadGameScript &script = menu_.script();
  DrawList list(textures_, font_, viewport, menu_.seconds());
  for (const auto &layer : script.layers) {
    list.sprite(layer);
  }
  const LoadGame::Phase phase = menu_.phase();
  if (phase != LoadGame::Phase::no_records) {
    for (std::size_t index = 0; index < script.slots.size(); ++index) {
      const SaveSlotStyle &slot = script.slots[index];
      const auto &quest = menu_.slots()[index].quest;
      list.sprite(slot.plate);
      if (static_cast<int>(index) == menu_.slot() && phase != LoadGame::Phase::exit) {
        // The lit plate pulses while the cursor is on the slots and holds
        // still behind a notice.
        Sprite lit = slot.lit;
        if (phase != LoadGame::Phase::slots) {
          lit.animation = Animation::none;
        }
        list.sprite(lit);
      }
      if (!quest) {
        list.sprite(slot.empty);
        continue;
      }
      if (const auto time = parse_saved_time(quest->saved)) {
        for (const auto &sprite : saved_time_sprites(script, slot, *time)) {
          list.sprite(sprite);
        }
      }
      if (quest->cleared) {
        list.sprite(slot.clear);
      }
    }
    list.sprite(script.exit.normal);
    if (phase == LoadGame::Phase::exit) {
      list.sprite(script.exit.selected);
    } else if (const auto &quest = menu_.slots()[static_cast<std::size_t>(menu_.slot())].quest) {
      draw_details(list, *quest);
    }
  }
  list.ticker(script.help, help_run_, menu_.seconds());
  frame_.draw(list, quest_);
  list.message_box(menu_.box());
  list.flush();
  fill_black(textures_, menu_.black_alpha());
}

} // namespace phantomcrash
