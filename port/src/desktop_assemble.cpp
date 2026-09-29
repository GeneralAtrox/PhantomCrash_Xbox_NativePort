#include "desktop_assemble.hpp"

#include "desktop_draw_list.hpp"
#include "desktop_shop_list.hpp"

#include <algorithm>
#include <utility>

namespace phantomcrash {
namespace {

Sprite placed(Sprite sprite, const std::array<float, 2> &at, const int layer) {
  sprite.position = at;
  sprite.layer = layer;
  return sprite;
}

// The picture at `index` of `pictures`, when there is one.
const Sprite *picture(const std::vector<Sprite> &pictures, const int index) {
  return index >= 0 && index < static_cast<int>(pictures.size())
             ? &pictures[static_cast<std::size_t>(index)]
             : nullptr;
}

} // namespace

AssembleScreen::AssembleScreen(TextureCache &textures, const GlyphFont &font, Assemble logic,
                               const MessageBoxStyle &box, const std::string &from,
                               const FrameStyle &frame, const std::vector<MusicCredit> &credits,
                               QuestSave quest)
    : TownScreen(textures, font, logic.script(), from, frame, credits, std::move(quest)),
      logic_(std::move(logic)), notice_(box, logic_.script().message_text_at) {}

void AssembleScreen::tick(const InputFrame &input, const double tick_seconds) {
  logic_.update(tick_seconds, input);
  tick_frame(tick_seconds, logic_.faded_in());
  notice_.update(logic_.message(), logic_.view() == AssembleView::question, logic_.choice(),
                 logic_.script().messages, tick_seconds);
  const QuestSave &now = logic_.quest();
  if (quest().riding != now.riding || quest().garage != now.garage) {
    set_quest(now);
  }
}

std::vector<std::string> AssembleScreen::take_sounds() {
  std::vector<std::string> sounds = logic_.take_sounds();
  for (std::string &name : notice_.take_sounds()) {
    sounds.push_back(std::move(name));
  }
  return sounds;
}

// 0x01877e with 0x06c530: the icons slide in, the chosen cell's lit copy
// over its icon (pulsing while the grid takes input), the disabled mark
// over what cannot be chosen and the shade over the grid behind a list.
void AssembleScreen::draw_grid(DrawList &list) const {
  const AssembleScript &script = logic_.script();
  const float slide = logic_.slide();
  const auto chosen = static_cast<std::size_t>(logic_.column() * script.grid_rows + logic_.row());
  for (std::size_t index = 0; index < script.cells.size(); ++index) {
    const AssembleCell &cell = script.cells[index];
    const float dx = (cell.from_x - cell.icon.position[0]) * (1.0F - slide);
    list.sprite(shifted(cell.icon, dx, 0.0F));
    if (index == chosen) {
      Sprite lit = shifted(cell.lit, dx, 0.0F);
      if (logic_.view() != AssembleView::grid) {
        lit.animation = Animation::none;
      }
      list.sprite(lit);
    }
    if (!logic_.can_choose(cell.item)) {
      list.sprite(placed(script.disabled, cell.icon.position, script.disabled.layer));
    }
  }
  if (logic_.view() != AssembleView::grid && logic_.view() != AssembleView::question) {
    list.sprite(script.list.shade, script.list.shade_alpha);
  }
}

// 0x01877e: the CHIP's face, the paint's and the emblem's pictures in their
// boxes, NO EQUIP where there is none.
void AssembleScreen::draw_equipment(DrawList &list) const {
  const AssembleScript &script = logic_.script();
  const AssemblePanel &panel = script.panel;
  const Hangar &fitted = logic_.fitted();
  const Garage &owned = logic_.quest().garage;
  list.sprite(panel.no_equip[0]);
  list.sprite(panel.no_equip[1]);
  if (const auto chip = owned.stock.find(fitted.part(PartSlot::chip)); chip != owned.stock.end()) {
    if (const ChipRecord *record = logic_.parts().chip(chip->second.id); record && record->face) {
      list.sprite(placed(*record->face, panel.chip_at, panel.equip_layer));
    }
  }
  const auto swatch = [&](const std::vector<PriceEntry> &entries, const int id,
                          const std::vector<Sprite> &swatches, const std::array<float, 2> &at) {
    if (id < 0 || id >= static_cast<int>(entries.size())) {
      return;
    }
    if (const Sprite *shown = picture(swatches, entries[static_cast<std::size_t>(id)].index)) {
      list.sprite(placed(*shown, at, panel.equip_layer));
    }
  };
  if (logic_.can_choose(AssembleItem::paint)) {
    swatch(logic_.garage().paints, fitted.paint, script.paint_swatches, panel.paint_at);
  }
  if (fitted.emblem == emblem_none) {
    list.sprite(panel.no_equip[2]);
  } else {
    swatch(logic_.garage().emblems, fitted.emblem, script.emblem_swatches, panel.emblem_at);
  }
}

// 0x01877e: the hangar's number, its sockets used and owned, its name and
// Equipment Imperfection, or Underkill and Overweight side by side.
void AssembleScreen::draw_panel(DrawList &list) const {
  const AssemblePanel &panel = logic_.script().panel;
  if (const Sprite *number = picture(panel.numbers, logic_.hangar())) {
    list.sprite(*number);
  }
  const RobotTotals totals = logic_.totals();
  const auto right_text = [&](const int value, const std::array<float, 2> &at) {
    const std::vector<int> run = glyph_run(font_, std::to_string(value));
    list.text(panel.sockets_layer, layout_run(font_, run, at[0] - run_width(font_, run), at[1]),
              panel.sockets_colour);
  };
  right_text(totals.sockets_used, panel.sockets_used);
  right_text(totals.sockets, panel.sockets_total);
  std::vector<int> name = glyph_run(font_, logic_.fitted().name);
  name.resize(std::min(name.size(), static_cast<std::size_t>(panel.name_glyphs)));
  list.text(panel.name_layer, layout_run(font_, name, panel.name_at[0], panel.name_at[1]),
            panel.name_colour);
  draw_equipment(list);
  const HangarStatus &status = logic_.status();
  if (status.empty) {
    return;
  }
  if (!status.complete) {
    list.sprite(panel.incomplete);
  } else if (status.unarmed && status.overweight) {
    Sprite under = panel.underkill;
    Sprite over = panel.overweight;
    under.position[0] = panel.pair_x[0];
    over.position[0] = panel.pair_x[1];
    list.sprite(under);
    list.sprite(over);
  } else if (status.unarmed) {
    list.sprite(panel.underkill);
  } else if (status.overweight) {
    list.sprite(panel.overweight);
  }
}

// 0x017af0: the pair rises out of the cell over the first half, then
// parts; the chosen side lit.
void AssembleScreen::draw_sides(DrawList &list) const {
  const AssembleSides &sides = *logic_.cell().sides;
  const float t = static_cast<float>(std::clamp(logic_.sides_seconds() / sides.seconds, 0.0, 1.0));
  const float rise = std::clamp(t * 2.0F, 0.0F, 1.0F) * sides.rise;
  const float spread = std::clamp((t - 0.5F) * 2.0F, 0.0F, 1.0F) * sides.spread;
  for (std::size_t side = 0; side < 2; ++side) {
    const float dx = side == 0 ? spread : -spread;
    list.sprite(shifted(sides.plain[side], dx, -rise));
    if (static_cast<int>(side) == logic_.side()) {
      list.sprite(shifted(sides.lit[side], dx, -rise));
    }
  }
}

void AssembleScreen::draw(const Viewport &viewport) {
  draw_town(viewport, logic_.seconds(), logic_.black_alpha(), [this](DrawList &list) {
    const AssembleScript &script = logic_.script();
    draw_grid(list);
    draw_panel(list);
    if (logic_.view() == AssembleView::sides && logic_.cell().sides) {
      draw_sides(list);
    }
    if (logic_.list_open()) {
      draw_list(list);
    }
    notice_.draw(list);
    list.ticker(script.help_style, glyph_run(font_, logic_.help_text()), logic_.help_seconds());
  });
}

} // namespace phantomcrash
