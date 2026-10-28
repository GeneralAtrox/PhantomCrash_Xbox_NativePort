#pragma once

#include "phantomcrash/rumbling.hpp"
#include "phantomcrash/rumbling_script.hpp"

#include <functional>

namespace phantomcrash {

class DrawList;

// What RUMBLING's drawing reads from its screen's logic, so S.L.A.I.'s area
// select (slai_area_select.hpp) draws its plates, sub-menu, map and gates
// exactly as RUMBLING does.
struct RumblingLook {
  int area{};
  RumblingView view{RumblingView::areas};
  float items_slide{};
  float bar_slide{};
  float turn{};
  bool in_menu{};
  int menu_item{};
  double menu_seconds{};
  int gate{};
  double seconds{};
  std::function<bool(RumblingAction)> can_choose;
};

// 0x015fce with 0x06c530: the bar and the area items slide in; the chosen
// item is lit over its own, pulsing while the areas take input. Turning to
// the sub-menu slides the areas back out; EXIT stays as the sub-menu's BACK.
void draw_rumbling_items(DrawList &list, const RumblingScript &script, const RumblingLook &look);

// 0x014edc: the sub-menu's items come in from the left as the screen turns;
// ENTRY and RENTAL are greyed where they cannot be chosen, the chosen one
// lit over its own (pulsing while the sub-menu takes input).
void draw_rumbling_menu_items(DrawList &list, const RumblingMenu &menu, const RumblingLook &look);

// 0x014af3 and 0x013bed: the map, the area's drawing building itself up a
// frame at a time (the frame going cross-faded and tinted), and its H marks.
// True once the drawing is up, for what goes over it.
bool draw_rumbling_map(DrawList &list, const RumblingMenu &menu, const RumblingArea &area,
                       const RumblingLook &look);

// 0x0150ea and 0x013a84: the gate window, a label for each of the stage's
// gates (the chosen one yellow), and the gates' marks on the map with a ring
// turning round the chosen one.
void draw_rumbling_gates(DrawList &list, const RumblingGates &gates, const RumblingArea &area,
                         const RumblingLook &look);

} // namespace phantomcrash
