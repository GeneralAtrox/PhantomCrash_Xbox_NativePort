#include "desktop_rumbling_look.hpp"

#include "desktop_draw_list.hpp"

#include <algorithm>
#include <cmath>

namespace phantomcrash {
namespace {

// How far the sub-menu's items trail each other as they come in (0x014edc).
constexpr float item_stagger = 32.0F;

Sprite shifted(Sprite sprite, const float dx) {
  sprite.position[0] += dx;
  return sprite;
}

Sprite placed(Sprite sprite, const CanvasPoint &at) {
  sprite.position = at;
  return sprite;
}

} // namespace

void draw_rumbling_items(DrawList &list, const RumblingScript &script, const RumblingLook &look) {
  const RumblingAreaSelect &style = script.area_select;
  list.sprite(shifted(style.bar, style.bar_from_x * (1.0F - look.bar_slide)));
  if (look.in_menu) {
    return;
  }
  const float out = std::max(1.0F - look.items_slide, look.turn);
  for (std::size_t index = 0; index < script.areas.size(); ++index) {
    const RumblingArea &area = script.areas[index];
    const float away = area.exit ? 1.0F - look.items_slide : out;
    const float dx = (area.item_from_x - area.item.position[0]) * away;
    list.sprite(shifted(area.item, dx));
    if (static_cast<int>(index) == look.area) {
      Sprite lit = shifted(area.item_lit, dx);
      if (look.view != RumblingView::areas) {
        lit.animation = Animation::none;
      }
      list.sprite(lit);
    }
  }
}

void draw_rumbling_menu_items(DrawList &list, const RumblingMenu &menu, const RumblingLook &look) {
  const float away = 1.0F - look.turn;
  for (std::size_t index = 0; index < menu.items.size(); ++index) {
    const RumblingMenuItem &item = menu.items[index];
    const bool back = item.action == RumblingAction::back;
    const float dx = back ? 0.0F : -(menu.item_slide + item_stagger * index) * away;
    Sprite normal = shifted(item.normal, dx);
    if (look.can_choose && !look.can_choose(item.action)) {
      normal.colour = menu.disabled_colour;
    }
    list.sprite(normal);
    if (static_cast<int>(index) == look.menu_item && look.turn >= 1.0F) {
      Sprite lit = shifted(item.lit, dx);
      if (look.view != RumblingView::menu) {
        lit.animation = Animation::none;
      }
      list.sprite(lit);
    }
  }
}

bool draw_rumbling_map(DrawList &list, const RumblingMenu &menu, const RumblingArea &area,
                       const RumblingLook &look) {
  list.sprite(menu.map, look.turn);
  if (look.turn < 1.0F || area.map_frames.empty()) {
    return false;
  }
  const double steps = look.menu_seconds / std::max(menu.map_frame_seconds, 1e-6);
  const int last = static_cast<int>(area.map_frames.size()) - 1;
  const int frame = std::min(static_cast<int>(steps), last);
  if (frame >= last) {
    list.sprite(area.map_frames[static_cast<std::size_t>(last)]);
  } else {
    const auto along = static_cast<float>(steps - std::floor(steps));
    if (frame > 0) {
      Sprite going = area.map_frames[static_cast<std::size_t>(frame - 1)];
      going.colour = menu.map_fade_colour;
      list.sprite(going, 1.0F - along);
    }
    list.sprite(area.map_frames[static_cast<std::size_t>(frame)], along);
  }
  for (const CanvasPoint &mark : area.marks) {
    list.sprite(placed(menu.mark, mark));
  }
  return true;
}

void draw_rumbling_gates(DrawList &list, const RumblingGates &gates, const RumblingArea &area,
                         const RumblingLook &look) {
  list.sprite(gates.window);
  for (const Sprite &hint : gates.hint) {
    list.sprite(hint);
  }
  const int count = static_cast<int>(area.gates.size());
  const auto first = gates.label_x.find(count);
  const float left = first != gates.label_x.end() ? first->second : gates.window.position[0];
  for (int gate = 0; gate < count && gate < static_cast<int>(gates.labels.size()); ++gate) {
    Sprite label = gates.labels[static_cast<std::size_t>(gate)];
    label.position[0] = left + gates.label_step * static_cast<float>(gate);
    if (gate == look.gate) {
      label.colour = gates.chosen_colour;
      label.layer = gates.chosen_layer;
    }
    list.sprite(label);
    list.sprite(placed(gates.marker, area.gates[static_cast<std::size_t>(gate)]));
  }
  if (look.gate < count) {
    const CanvasPoint &at = area.gates[static_cast<std::size_t>(look.gate)];
    list.sprite(placed(gates.marker_lit, at));
    Sprite ring = placed(gates.ring, at);
    ring.angle =
        std::fmod(static_cast<float>(look.seconds) * gates.ring_degrees_per_second, 360.0F);
    list.sprite(ring);
  }
}

} // namespace phantomcrash
