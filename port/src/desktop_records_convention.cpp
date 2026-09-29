// RECORDS' CONVENTION RECORD: the menu of venues (0x34451) and a venue's or
// REGULATION's grid of contests with the cursor's champion (0x33660,
// 0x3472d).

#include "desktop_records.hpp"

#include "desktop_draw_list.hpp"

namespace phantomcrash {
namespace {

// The venue grid's large cell, the area ranker (0x330d1).
constexpr int area_ranker_column = 4;

Sprite placed(Sprite sprite, const std::array<float, 2> &at) {
  sprite.position = at;
  return sprite;
}

} // namespace

void RecordsScreen::draw_convention_items(DrawList &list, const bool sliding) const {
  const ConventionStyle &style = records_.script().convention;
  for (int item = 0; item < static_cast<int>(style.items.size()); ++item) {
    Sprite sprite = style.items[static_cast<std::size_t>(item)];
    sprite.position[0] += sliding ? convention_slide(style, item, records_.view_seconds()) : 0.0F;
    const bool lit = item == records_.convention_cursor();
    if (lit) {
      sprite.colour = records_.script().menu.lit_colour;
      sprite.animation = Animation::pulse;
    } else {
      // Unlit items are white in the menu, dimmed once one is open.
      sprite.colour = records_.view() == RecordsView::convention ? 0xffffff : style.dim_colour;
    }
    list.sprite(sprite);
  }
}

// The cursor's contest: its champion team's name and portraits, crossed
// LOSER once won, and the contest's name.
void RecordsScreen::draw_champion(DrawList &list, const int contest, const int slot) const {
  const ConventionStyle &style = records_.script().convention;
  const Contest *entry = contests_.contest(contest);
  if (entry == nullptr) {
    return;
  }
  const bool hidden = records_.champion_hidden(contest);
  const auto &names = records_.venue().champion_names;
  if (hidden) {
    list.sprite(style.hidden.name);
  } else if (slot >= 0 && slot < static_cast<int>(names.size())) {
    list.sprite(names[static_cast<std::size_t>(slot)]);
  }
  const ConventionRegulation &regulation = style.regulation;
  const bool own =
      records_.view() == RecordsView::regulation && slot == regulation.own_portrait_slot;
  const std::vector<Sprite> &portraits =
      hidden ? style.hidden.portraits : entry->champion_portraits;
  for (std::size_t index = 0; index < style.portraits.size(); ++index) {
    const bool player = own && index == 0;
    if (!player && index >= portraits.size()) {
      continue;
    }
    Sprite portrait =
        placed(player ? regulation.own_portrait : portraits[index], style.portraits[index]);
    portrait.layer = style.portrait_layer;
    list.sprite(portrait);
    if (records_.won(contest) && !player) {
      list.sprite(placed(style.loser, style.portraits[index]));
    }
  }
  list.text(style.text_layer,
            layout_run(font_, glyph_run(font_, entry->name), style.contest_name[0],
                       style.contest_name[1]),
            style.text_colour);
}

void RecordsScreen::draw_grid(DrawList &list) const {
  const ConventionStyle &style = records_.script().convention;
  const bool regulation = records_.view() == RecordsView::regulation;
  list.sprite(regulation ? style.regulation.grid : style.grid);
  const float growing = growing_after(style.growth, records_.view_seconds());
  list.sprite(grown_panel(style.panel, growing, style.growth.from_scale));
  if (regulation) {
    const int flipping = records_.flipping();
    for (std::size_t side = 0; side < style.regulation.arrows.size(); ++side) {
      const bool lit = flipping == (side == 0 ? -1 : 1);
      list.sprite(lit ? style.regulation.arrows_lit[side] : style.regulation.arrows[side]);
    }
  }
  if (growing > 0.0F) {
    return;
  }
  for (const Sprite &hint : style.hint) {
    list.sprite(hint);
  }
  const int column = records_.grid_column();
  const int row = records_.grid_row();
  const ConventionVenue &venue = records_.venue();
  if (!regulation && column == area_ranker_column) {
    list.sprite(style.area_cursor);
    list.sprite(venue.header);
  } else {
    list.sprite(placed(style.cursor,
                       {style.cursor_origin[0] + style.cursor_step * static_cast<float>(column),
                        style.cursor_origin[1] + style.cursor_step * static_cast<float>(row)}));
    if (regulation) {
      list.sprite(venue.header);
    } else {
      list.sprite(style.day_night[static_cast<std::size_t>(row)]);
      if (column < static_cast<int>(venue.ranks.size())) {
        list.sprite(venue.ranks[static_cast<std::size_t>(column)]);
      }
    }
  }
  draw_champion(list, records_.selected_contest(), records_.slot());
}

void RecordsScreen::draw_convention(DrawList &list) const {
  const ConventionStyle &style = records_.script().convention;
  if (records_.view() != RecordsView::convention) {
    draw_convention_items(list, false);
    draw_grid(list);
    return;
  }
  // 0x34451: the ellipse grows, the items slide in, then the FIRST RANKER
  // plate shows its champion.
  const float growing = growing_after(style.ellipse_growth, records_.view_seconds());
  list.sprite(grown_panel(style.ellipse, growing, style.ellipse_growth.from_scale));
  list.sprite(style.first_ranker);
  draw_convention_items(list, true);
  if (records_.view_seconds() < style.slide_seconds) {
    return;
  }
  const Contest *first = contests_.contest(style.first_ranker_contest);
  if (first == nullptr) {
    return;
  }
  const bool won = records_.won(style.first_ranker_contest);
  for (std::size_t index = 0;
       index < first->champion_portraits.size() && index < style.first_ranker_portraits.size();
       ++index) {
    Sprite portrait = placed(first->champion_portraits[index], style.first_ranker_portraits[index]);
    portrait.layer = style.portrait_layer;
    list.sprite(portrait);
    if (won) {
      list.sprite(placed(style.loser, style.first_ranker_portraits[index]));
    }
  }
}

} // namespace phantomcrash
