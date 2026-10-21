#pragma once

#include "desktop_draw_list.hpp"
#include "phantomcrash/town_map.hpp"

namespace phantomcrash {

// The widescreen town map's vector art (TownMapScript's bands, lettering
// and markers), drawn over the map in view units.
void draw_town_bands(DrawList &list, const TownMapScript &script);
void draw_town_lettering(DrawList &list, const TownMapScript &script);
// Every place's marker, numbered in the order shown; the `chosen` place's
// face and arrows are drawn at `pulse` (its pulsing alpha).
void draw_town_markers(DrawList &list, const TownMapScript &script, int chosen, float pulse);

} // namespace phantomcrash
