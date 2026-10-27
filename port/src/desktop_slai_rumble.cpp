#include "desktop_slai_rumble.hpp"

#include "desktop_contest_info.hpp"
#include "desktop_draw_list.hpp"
#include "desktop_schedule.hpp"

#include <algorithm>
#include <utility>

namespace phantomcrash {
namespace {

constexpr std::uint32_t lit_colour = 0xffffff;

std::array<float, 4> offset_box(const std::array<float, 4> &box, const float x, const float y) {
  return {box[0] + x, box[1] + y, box[2] + x, box[3] + y};
}

void draw_label(DrawList &list, const SlaiScheduleLabel &label, std::string text,
                const std::array<float, 4> &box, const bool centered) {
  DisplayLayout layout;
  layout.centered = centered;
  list.label(label.layer, std::move(text), label.font, box, label.colour, label.shadow,
             label.shadow_blur, layout);
}

} // namespace

SlaiRumbleScreen::SlaiRumbleScreen(TextureCache &textures, const GlyphFont &font,
                                   SlaiRumbleHubScript script, const SlaiRumbleData &data,
                                   const std::string &from, const std::string &back,
                                   const FrameStyle &frame, const std::vector<MusicCredit> &credits,
                                   const QuestSave &quest, const MessageBoxStyle &box,
                                   SlaiRobot riding, const bool rentals)
    : TownScreen(textures, font, script, from, frame, credits, quest), money_font_(frame.hud.font),
      hub_(std::move(script), data, quest, std::move(riding), rentals, back, box) {
  static_cast<void>(textures_.get(hub_.script().cell_image));
}

void SlaiRumbleScreen::tick(const InputFrame &input, const double tick_seconds) {
  hub_.update(tick_seconds, input);
  tick_frame(tick_seconds, hub_.faded_in());
}

// A city's row: its plate and name, the venue's tile (day or night, lit
// under the cursor) with the regulation's badge and CLEAR once its record
// is, the regulation's name, and the money it needs and pays.
void SlaiRumbleScreen::draw_row(DrawList &list, const int row) const {
  const SlaiRumbleHubScript &script = hub_.script();
  const SlaiRumbleData &data = hub_.data();
  const SlaiCityMatch &shown = hub_.matches()[static_cast<std::size_t>(row)];
  const SlaiMatch *match = slai_match(data.table, shown.match);
  const SlaiRegulation *rule = slai_regulation(data.table, match->regulation);
  const float x = script.row_origin[0] + script.row_step[0] * static_cast<float>(row);
  const float y = script.row_origin[1] + script.row_step[1] * static_cast<float>(row);
  const bool lit = row == hub_.row();

  draw_city(list, shown, x, y);

  Sprite tile;
  tile.image = script.cell_image;
  tile.position = {x + script.tile_at[0], y + script.tile_at[1]};
  tile.layer = script.tile_layer;
  const ScheduleVenueTiles &tiles = script.tiles;
  tile.rect =
      shown.night ? (lit ? tiles.night_lit : tiles.night) : (lit ? tiles.day_lit : tiles.day);
  list.sprite(tile);
  if (const SlaiHubBadge *badge = slai_hub_badge(script, rule->id)) {
    Sprite mark = tile;
    mark.rect = badge->rect;
    mark.layer = script.badge_layer;
    mark.colour = lit ? lit_colour : script.badge_dim;
    list.sprite(mark);
  }
  if (slai_record_cleared(hub_.quest().slai_rumble, match->save_record)) {
    Sprite clear = script.clear;
    clear.position = tile.position;
    list.sprite(clear);
  }

  draw_label(list, script.rule, rule->name, offset_box(script.rule.box, x, y), false);
  draw_label(list, script.entry_label, script.entry_text, offset_box(script.entry_label.box, x, y),
             false);
  draw_label(list, script.prize_label, script.prize_text, offset_box(script.prize_label.box, x, y),
             false);
  const std::array<std::pair<std::array<float, 2>, std::int64_t>, 2> amounts{
      {{script.entry_at, slai_entry_requirement(data.world, *rule)},
       {script.prize_at, slai_prize(data.world, *rule, rule->target)}}};
  for (const auto &[at, amount] : amounts) {
    draw_amount(list, money_font_, {x + at[0], y + at[1]}, script.amount_digits, amount,
                script.info.fee_unit, script.amount_colour, script.amount_layer);
  }
}

void SlaiRumbleScreen::draw_city(DrawList &list, const SlaiCityMatch &shown, const float x,
                                 const float y) const {
  const SlaiRumbleHubScript &script = hub_.script();
  const auto city = static_cast<std::size_t>(shown.city);
  if (city < script.city_art.size() && textures_.get(script.city_art[city].image) != nullptr) {
    const SlaiCityArt &art = script.city_art[city];
    Sprite emblem;
    emblem.image = art.image;
    emblem.rect = art.emblem;
    emblem.layer = script.art_layer;
    const std::array<float, 4> &box = script.emblem_box;
    const float width = art.emblem[2] - art.emblem[0];
    const float height = art.emblem[3] - art.emblem[1];
    const float scale = std::min((box[2] - box[0]) / width, (box[3] - box[1]) / height);
    emblem.size = std::array<float, 2>{width * scale, height * scale};
    emblem.position = {(box[0] + box[2] - width * scale) * 0.5F,
                       y + (box[1] + box[3] - height * scale) * 0.5F};
    list.sprite(emblem);
    Sprite label = emblem;
    label.rect = shown.night ? art.night : art.day;
    label.size = std::array<float, 2>{(label.rect[2] - label.rect[0]) * script.art_label_scale,
                                      (label.rect[3] - label.rect[1]) * script.art_label_scale};
    label.position = {x + script.art_label_at[0], y + script.art_label_at[1]};
    list.sprite(label);
    return;
  }
  const std::array<float, 4> plate = offset_box(script.plate.box, 0.0F, y);
  list.shape(script.plate.layer, script.plate.outlines, plate);
  const SlaiRumbleData &data = hub_.data();
  draw_label(list, script.city,
             city < script.city_names.size() ? script.city_names[city] : data.table.cities[city],
             offset_box(script.city.box, plate[0], plate[1]), true);
}

void SlaiRumbleScreen::draw_cursor(DrawList &list) const {
  const SlaiRumbleHubScript &script = hub_.script();
  list.sprite(script.exit);
  if (hub_.row() == hub_.exit_row()) {
    Sprite lit = script.exit_selected;
    lit.position = script.exit.position;
    list.sprite(lit);
    return;
  }
  if (hub_.popup().is_open()) {
    return;
  }
  const Sprite box = hub_.row_box(hub_.row());
  const std::array<float, 2> size{box.rect[2], box.rect[3]};
  nine_slice(list, script.cursor_frame, box.position, size, script.cursor_border);
  nine_slice(list, script.cursor_pulse, box.position, size, script.cursor_border);
}

// The rumble's INFORMATION, grown out of its row.
void SlaiRumbleScreen::draw_popup(DrawList &list) const {
  const SlaiRumbleHubScript &script = hub_.script();
  const Sprite box = hub_.row_box(hub_.row());
  const std::array<float, 2> centre{box.position[0] + box.rect[2] * 0.5F,
                                    box.position[1] + box.rect[3] * 0.5F};
  list.sprite(grown_panel(script.info.panel, hub_.popup(), centre));
  const SlaiCityMatch *shown = hub_.selected();
  if (!hub_.popup().grown() || shown == nullptr) {
    return;
  }
  const SlaiRumbleData &data = hub_.data();
  const SlaiRegulation *rule =
      slai_regulation(data.table, slai_match(data.table, shown->match)->regulation);
  const SlaiHubBadge *badge = slai_hub_badge(script, rule->id);
  draw_slai_rule_info(list, font_, money_font_, script.info, data, *rule,
                      badge != nullptr ? badge->class_text : std::string{});
}

void SlaiRumbleScreen::draw(const Viewport &viewport) {
  draw_town(viewport, hub_.seconds(), hub_.black_alpha(), [this](DrawList &list) {
    const SlaiRumbleHubScript &script = hub_.script();
    for (int row = 0; row < hub_.exit_row(); ++row) {
      draw_row(list, row);
    }
    if (hub_.matches().empty()) {
      float y = script.rule.box[1] + script.row_origin[1];
      const auto &lines =
          hub_.only_excluded() && !script.excluded.empty() ? script.excluded : script.closed;
      for (const std::string &line : lines) {
        draw_label(list, script.rule, line,
                   {script.row_origin[0], y, script.row_origin[0] + script.rule.box[2],
                    y + script.rule.box[3] - script.rule.box[1]},
                   false);
        y += script.row_step[1] * 0.5F;
      }
    }
    draw_cursor(list);
    if (hub_.popup().is_open()) {
      draw_popup(list);
    }
    if (hub_.box().is_open()) {
      list.message_box(hub_.box());
    }
  });
}

} // namespace phantomcrash
