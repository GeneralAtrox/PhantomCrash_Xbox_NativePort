// VsModeScreen's area select (0x03fcc9): the backdrop and strips, the
// picture cross-fading with its label and title, the items sliding in with
// the bar under them, and the time-zone window.

#include "desktop_draw_list.hpp"
#include "desktop_vs_mode.hpp"

#include <algorithm>

namespace phantomcrash {

void VsModeScreen::draw_area(DrawList &list) const {
  const VsAreaSelect &area = logic_.script().area;
  for (const Sprite &sprite : area.layers) {
    list.sprite(sprite);
  }
  list.sprite(area.picture_frame);
  draw_pictures(list);
  draw_area_items(list);
  draw_zone(list);
}

// Each item slides from its from_x to its place; the bar ends under EXIT.
void VsModeScreen::draw_area_items(DrawList &list) const {
  const VsAreaSelect &area = logic_.script().area;
  const std::vector<int> &items = logic_.items();
  const bool crowd = items.size() < area.items.size() && items.size() == area.crowd.xs.size();
  const float slide = logic_.slide_along();
  float exit_x = 0.0F;
  for (std::size_t shown = 0; shown < items.size(); ++shown) {
    const VsAreaItem &item = area.items[static_cast<std::size_t>(items[shown])];
    const float to = crowd ? area.crowd.xs[shown] : item.item.normal.position[0];
    const float from = crowd ? area.crowd.from_xs[shown] : item.from_x;
    const float x = from + (to - from) * slide;
    Sprite normal = item.item.normal;
    normal.position[0] = x;
    list.sprite(normal);
    if (static_cast<int>(shown) == logic_.area_cursor()) {
      Sprite lit = item.item.selected;
      lit.position[0] = x;
      list.sprite(lit);
    }
    if (item.kind == VsAreaKind::exit) {
      exit_x = x;
    }
  }
  Sprite bar = area.bar;
  bar.position[0] = exit_x - area.bar_offset;
  list.sprite(bar);
}

// The picture before stays under the new one fading in; their labels and
// titles cross-fade. RANDOM shows its changing stage picture.
void VsModeScreen::draw_pictures(DrawList &list) const {
  const VsAreaSelect &area = logic_.script().area;
  const float along = logic_.picture_along();
  const auto draw_one = [&](const VsMode::Picture &picture, const float alpha,
                            const float picture_alpha) {
    if (picture.item < 0 || alpha <= 0.0F) {
      return;
    }
    const VsAreaItem &item = area.items[static_cast<std::size_t>(picture.item)];
    const Sprite *shown = picture.night ? &item.night_picture : &item.picture;
    if (item.kind == VsAreaKind::random && !area.random_pictures.empty()) {
      const int stage =
          std::clamp(logic_.random_stage(), 0, static_cast<int>(area.random_pictures.size()) - 1);
      shown = &area.random_pictures[static_cast<std::size_t>(stage)];
    }
    list.sprite(*shown, picture_alpha);
    list.sprite(item.label, alpha);
    list.sprite(item.title, alpha);
  };
  if (along < 1.0F) {
    draw_one(logic_.previous_picture(), 1.0F - along, 1.0F);
  }
  draw_one(logic_.picture(), along, along);
}

// 0x1ffb1c: the window opens from its middle; the time chosen pulses, the
// other is dimmed.
void VsModeScreen::draw_zone(DrawList &list) const {
  const VsTimeZone &zone = logic_.script().area.time_zone;
  const float along = logic_.zone().along();
  if (along <= 0.0F) {
    return;
  }
  list.sprite(scaled_sprite(zone.window, 1.0F, along));
  if (along < 1.0F) {
    return;
  }
  const ChoiceSprites *options[] = {&zone.day, &zone.night};
  for (int option = 0; option < 2; ++option) {
    if (option == logic_.zone_choice()) {
      list.sprite(options[option]->selected);
    } else {
      Sprite dim = options[option]->normal;
      dim.colour = zone.dim_colour;
      list.sprite(dim);
    }
  }
}

} // namespace phantomcrash
