// The schedule's S.L.A.I. page (phantomcrash/schedule_slai.hpp): the
// Phantom Crash page's art cut to seven thinner rows, with sharp plates,
// lines and lettering.

#include "desktop_schedule.hpp"

#include "desktop_contest_info.hpp"
#include "desktop_draw_list.hpp"

#include <algorithm>
#include <utility>

namespace phantomcrash {
namespace {

constexpr std::uint32_t lit_colour = 0xffffff;

float width_of(const std::array<float, 4> &rect) {
  return rect[2] - rect[0];
}

float height_of(const std::array<float, 4> &rect) {
  return rect[3] - rect[1];
}

// `rect` of the cell image scaled to fit `box` (canvas LTRB), centred.
Sprite fitted(Sprite sprite, const std::array<float, 4> &rect, const std::array<float, 4> &box) {
  const float scale = std::min(width_of(box) / width_of(rect), height_of(box) / height_of(rect));
  const std::array<float, 2> size{width_of(rect) * scale, height_of(rect) * scale};
  sprite.rect = rect;
  sprite.size = size;
  sprite.position = {box[0] + (width_of(box) - size[0]) * 0.5F,
                     box[1] + (height_of(box) - size[1]) * 0.5F};
  return sprite;
}

std::array<float, 4> offset_box(const std::array<float, 4> &box, const float x, const float y) {
  return {box[0] + x, box[1] + y, box[2] + x, box[3] + y};
}

void draw_label(DrawList &list, const SlaiScheduleLabel &label, std::string text,
                const std::array<float, 4> &box) {
  DisplayLayout layout;
  layout.centered = true;
  list.label(label.layer, std::move(text), label.font, box, label.colour, label.shadow,
             label.shadow_blur, layout);
}

} // namespace

void nine_slice(DrawList &list, const Sprite &sprite, const std::array<float, 2> &at,
                const std::array<float, 2> &size, const float border) {
  const auto &rect = sprite.rect;
  const std::array<float, 4> from_x{rect[0], rect[0] + border, rect[2] - border, rect[2]};
  const std::array<float, 4> from_y{rect[1], rect[1] + border, rect[3] - border, rect[3]};
  const std::array<float, 4> to_x{at[0], at[0] + border, at[0] + size[0] - border, at[0] + size[0]};
  const std::array<float, 4> to_y{at[1], at[1] + border, at[1] + size[1] - border, at[1] + size[1]};
  for (std::size_t column = 0; column < 3; ++column) {
    for (std::size_t row = 0; row < 3; ++row) {
      const std::array<float, 2> piece_size{to_x[column + 1] - to_x[column],
                                            to_y[row + 1] - to_y[row]};
      if (piece_size[0] <= 0.0F || piece_size[1] <= 0.0F) {
        continue;
      }
      Sprite piece = sprite;
      piece.rect = {from_x[column], from_y[row], from_x[column + 1], from_y[row + 1]};
      piece.position = {to_x[column], to_y[row]};
      piece.size = piece_size;
      list.sprite(piece);
    }
  }
}

void draw_slai_rule_info(DrawList &list, const GlyphFont &font, const NumberFont &fee_font,
                         const ContestInfoStyle &info, const SlaiRumbleData &data,
                         const SlaiRegulation &rule, const std::string &class_text) {
  const auto text = [&](const std::string &line, const std::array<float, 2> &at) {
    list.text(info.text_layer, layout_run(font, glyph_run(font, line), at[0], at[1]),
              info.text_colour);
  };
  const auto rule_text = [&](const bool has) { return has ? rule.name : info.no_limits; };
  const bool legs = std::ranges::any_of(rule.leg_classes, [&](const std::string &name) {
    return std::ranges::any_of(data.world.leg_classes,
                               [&](const auto &known) { return known.first == name; });
  });
  text(rule.name, info.contest);
  if (info.contest_class) {
    text(class_text, *info.contest_class);
  }
  text(rule_text(rule.maker != slai_any_maker), info.body);
  text(rule_text(legs), info.legs);
  text(rule_text(rule.special != SlaiSpecialRule::none), info.other);
  draw_amount(list, fee_font, info.fee, info.fee_digits, slai_entry_requirement(data.world, rule),
              info.fee_unit, info.text_colour, info.text_layer);
}

// The DAY- strip of the Phantom Crash grid over the columns, and each
// column's day in its digits.
void ScheduleScreen::draw_slai_days(DrawList &list) const {
  const ScheduleScript &data = schedule_.script();
  const SlaiScheduleScript &slai = data.slai;
  list.sprite(slai.header);
  const ScheduleDigits &digits = data.day_digits;
  for (int column = 0; column < data.columns; ++column) {
    const int day = (schedule_.first_day() + column) % calendar_days + 1;
    const float step = slai.cell_step[0] * static_cast<float>(column);
    const std::array<std::pair<std::array<float, 2>, int>, 2> places{
        {{slai.tens, day / 10}, {slai.units, day % 10}}};
    for (const auto &[place, digit] : places) {
      Sprite sprite;
      sprite.image = digits.image;
      sprite.rect = digits.digits[static_cast<std::size_t>(digit)];
      sprite.position = {place[0] + step, place[1]};
      sprite.layer = digits.layer;
      list.sprite(sprite);
    }
  }
}

// Each city's cells: its venue strip (day or night, lit in the cursor's
// column) or CLOSED, the regulation's badge and CLEAR once its record is.
void ScheduleScreen::draw_slai_cells(DrawList &list) const {
  const ScheduleScript &data = schedule_.script();
  const SlaiScheduleScript &slai = data.slai;
  const SlaiRumbleData &rumble = *schedule_.slai();
  for (int column = 0; column < data.columns; ++column) {
    const ScheduleColumn view = schedule_.column_view(column);
    if (!view.shown) {
      continue;
    }
    const bool lit = column == schedule_.column();
    for (int row = 0; row < schedule_.rows(); ++row) {
      const Sprite cell_box = schedule_.cell_box(column, row);
      const float left = cell_box.position[0] + view.offset_x;
      const float top = cell_box.position[1];
      const auto shown = schedule_.slai_match_at(view.day, row);
      const SlaiMatch *match = shown ? slai_match(rumble.table, shown->match) : nullptr;
      Sprite tile;
      tile.image = data.cell_image;
      tile.position = {left, top};
      tile.layer = data.tile_layer;
      tile.rect = lit ? slai.closed_lit : slai.closed;
      if (match != nullptr) {
        tile.rect =
            shown->night ? (lit ? slai.night_lit : slai.night) : (lit ? slai.day_lit : slai.day);
      }
      fold(tile, view.scale_x);
      list.sprite(tile);
      if (match == nullptr) {
        continue;
      }
      if (const SlaiScheduleBadge *badge = slai_schedule_badge(slai, match->regulation)) {
        const float scale = std::min(slai.badge_height / height_of(badge->rect),
                                     slai.badge_width / width_of(badge->rect));
        const float height = height_of(badge->rect) * scale;
        Sprite mark = tile;
        mark.rect = badge->rect;
        mark.size = std::array<float, 2>{width_of(badge->rect) * scale, height};
        mark.position = {left + slai.badge_inset, top + (slai.cell_step[1] - height) * 0.5F};
        mark.layer = data.badge_layer;
        mark.colour = lit && !schedule_.scrolling() ? lit_colour : slai.badge_dim;
        fold(mark, view.scale_x);
        list.sprite(mark);
      }
      if (slai_record_cleared(schedule_.quest().slai_rumble, match->save_record)) {
        Sprite clear = fitted(data.clear, slai.clear_rect, offset_box(slai.clear_box, left, top));
        fold(clear, view.scale_x);
        list.sprite(clear);
      }
    }
  }
}

// The grid's lines, each row's city plate and the page's own name.
void ScheduleScreen::draw_slai_plates(DrawList &list) const {
  const SlaiScheduleScript &slai = schedule_.script().slai;
  const SlaiRumbleTable &table = schedule_.slai()->table;
  list.shape(slai.grid.layer, slai.grid.outlines, slai.grid.box);
  for (int row = 0; row < schedule_.rows(); ++row) {
    const float top = slai.cell_origin[1] + slai.cell_step[1] * static_cast<float>(row);
    const std::array<float, 4> plate = offset_box(slai.plate.box, 0.0F, top);
    list.shape(slai.plate.layer, slai.plate.outlines, plate);
    const auto city = static_cast<std::size_t>(schedule_.slai_city(row));
    const std::string &name =
        city < slai.city_names.size() ? slai.city_names[city] : table.cities[city];
    draw_label(list, slai.city, name, offset_box(slai.city.box, plate[0], plate[1]));
  }
  list.shape(slai.corner.layer, slai.corner.outlines, slai.corner.box);
  draw_label(list, slai.title, slai.title_text,
             offset_box(slai.title.box, slai.corner.box[0], slai.corner.box[1]));
}

// The Phantom Crash cursor's frame and pulse fitted to the thinner cell;
// on SKIP TO DAY, the day it wakes on stays marked.
void ScheduleScreen::draw_slai_cursor(DrawList &list) const {
  const ScheduleScript &data = schedule_.script();
  const SlaiScheduleScript &slai = data.slai;
  const auto mark_day = [&](const float x) {
    Sprite day = data.cursor_day;
    day.position = {x, slai.cursor_day_y};
    list.sprite(day);
  };
  const Sprite box = schedule_.cell_box(schedule_.column(), 0);
  if (schedule_.row() == schedule_.button_row()) {
    if (schedule_.button() == Schedule::skip_button) {
      mark_day(box.position[0]);
    }
    return;
  }
  if (schedule_.popup().is_open() || schedule_.scrolling()) {
    return;
  }
  const Sprite cell = schedule_.cell_box(schedule_.column(), schedule_.row());
  const std::array<float, 2> at{cell.position[0] + slai.cursor_offset[0],
                                cell.position[1] + slai.cursor_offset[1]};
  nine_slice(list, data.cursor_frame, at, slai.cursor_size, slai.cursor_border);
  nine_slice(list, data.cursor_pulse, at, slai.cursor_size, slai.cursor_border);
  mark_day(cell.position[0]);
}

// The rumble's INFORMATION, grown out of its cell: the regulation's own
// name, its class, the rule it sets where it has one (its name again, the
// S.L.A.I. text) or "No limits.", and the money entry needs.
void ScheduleScreen::draw_slai_info(DrawList &list) const {
  const ScheduleScript &data = schedule_.script();
  const ContestInfoStyle &info = data.info;
  const SlaiRumbleData &rumble = *schedule_.slai();
  const Sprite cell = schedule_.cell_box(schedule_.column(), schedule_.row());
  const std::array<float, 2> centre{cell.position[0] + data.slai.cell_step[0] * 0.5F,
                                    cell.position[1] + data.slai.cell_step[1] * 0.5F};
  list.sprite(grown_panel(info.panel, schedule_.popup(), centre));
  const auto shown = schedule_.selected_slai();
  const SlaiMatch *match = shown ? slai_match(rumble.table, shown->match) : nullptr;
  const SlaiRegulation *rule =
      match != nullptr ? slai_regulation(rumble.table, match->regulation) : nullptr;
  if (!schedule_.popup().grown() || rule == nullptr) {
    return;
  }
  const SlaiScheduleBadge *badge = slai_schedule_badge(data.slai, rule->id);
  draw_slai_rule_info(list, font_, fee_font_, info, rumble, *rule,
                      badge != nullptr ? badge->class_text : std::string{});
}

std::string ScheduleScreen::slai_ticker_text() const {
  const auto shown = schedule_.selected_slai();
  if (!shown) {
    return {};
  }
  const SlaiRumbleTable &table = schedule_.slai()->table;
  const SlaiMatch *match = slai_match(table, shown->match);
  const SlaiRegulation *rule =
      match != nullptr ? slai_regulation(table, match->regulation) : nullptr;
  return rule != nullptr ? rule->name : std::string{};
}

} // namespace phantomcrash
