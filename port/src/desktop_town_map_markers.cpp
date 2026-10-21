#include "desktop_town_map_markers.hpp"

#include <algorithm>
#include <cmath>
#include <cstdio>
#include <numbers>
#include <utility>
#include <vector>

namespace phantomcrash {
namespace {

constexpr int ellipse_points = 96;

// A filled outline in `colour`, its edge antialiased.
std::vector<ShapeStop> solid(const std::uint32_t colour) {
  return {{0.0F, colour, 1.0F}, {0.0F, colour, 0.0F}};
}

// A soft shadow: `alpha` black fading out across `blur` either side of the edge.
std::vector<ShapeStop> soft(const float alpha, const float blur) {
  return {{-blur, 0x000000, alpha}, {blur, 0x000000, 0.0F}};
}

ShapeOutline polygon(std::vector<std::array<float, 2>> points, std::vector<ShapeStop> stops) {
  ShapeOutline outline;
  outline.box = {1e9F, 1e9F, -1e9F, -1e9F};
  for (const auto &[x, y] : points) {
    outline.box = {std::min(outline.box[0], x), std::min(outline.box[1], y),
                   std::max(outline.box[2], x), std::max(outline.box[3], y)};
  }
  outline.path = std::move(points);
  outline.stops = std::move(stops);
  return outline;
}

ShapeOutline ellipse(const std::array<float, 2> &centre, const std::array<float, 2> &radii,
                     std::vector<ShapeStop> stops) {
  std::vector<std::array<float, 2>> points;
  for (int index = 0; index < ellipse_points; ++index) {
    const double angle = 2.0 * std::numbers::pi * index / ellipse_points;
    points.push_back({centre[0] + radii[0] * static_cast<float>(std::cos(angle)),
                      centre[1] + radii[1] * static_cast<float>(std::sin(angle))});
  }
  return polygon(std::move(points), std::move(stops));
}

ShapeOutline rectangle(const std::array<float, 4> &box, std::vector<ShapeStop> stops) {
  ShapeOutline outline;
  outline.box = box;
  outline.stops = std::move(stops);
  return outline;
}

std::array<float, 2> shifted(const std::array<float, 2> &point, const std::array<float, 2> &by) {
  return {point[0] + by[0], point[1] + by[1]};
}

std::array<float, 2> inset(const std::array<float, 2> &radii, const float by) {
  return {radii[0] - by, radii[1] - by};
}

// The red arrows either side of the chosen disc, pointing out.
std::vector<std::array<float, 2>> arrow(const TownMarkerStyle &style,
                                        const std::array<float, 2> &spot, const float way) {
  const float base = spot[0] + way * (style.radii[0] + style.arrow_gap);
  const float tip = base + way * style.arrow_reach;
  return {{base, spot[1] - style.arrow_half}, {tip, spot[1]}, {base, spot[1] + style.arrow_half}};
}

// The place's label: from the disc's side out, as the boxes place it.
struct Label {
  float left{};
  float right{};
};

Label label_of(const TownMarkerStyle &style, const TownPlace &place) {
  const float width = std::max(place.label_width, style.min_label_width);
  const float left = place.side > 0 ? place.spot[0] + style.radii[0] + style.label_gap
                                    : place.spot[0] - style.radii[0] - style.label_gap - width;
  return {left, left + width};
}

std::vector<ShapeOutline> bar_code(const TownMarkerStyle &style, const TownPlace &place,
                                   const Label &label) {
  std::vector<ShapeOutline> bars;
  int modules = 0;
  for (const char width : place.bars) {
    modules += width - '0';
  }
  if (modules == 0) {
    return bars;
  }
  const float unit = (label.right - label.left) / static_cast<float>(modules);
  const float top = place.spot[1] + style.bars_top;
  const float bottom = place.spot[1] + style.bars_bottom;
  float x = label.left;
  for (std::size_t index = 0; index < place.bars.size(); ++index) {
    const float width = static_cast<float>(place.bars[index] - '0') * unit;
    if (index % 2 == 0) {
      bars.push_back(rectangle({x, top, x + width - style.bar_trim, bottom}, solid(style.rim)));
    }
    x += width;
  }
  return bars;
}

// Display lettering's edges in canvas units from view units.
LabelEdge edge_of(const TownView &view, const float bold, const std::uint32_t colour,
                  const std::optional<TextShadow> &shadow, const float blur) {
  LabelEdge edge;
  if (bold > 0.0F) {
    edge.outline = colour;
    edge.outline_width = bold * view.scale();
  }
  if (shadow) {
    TextShadow drop = *shadow;
    drop.offset = {drop.offset[0] * view.scale(), drop.offset[1] * view.scale()};
    edge.shadow = drop;
    edge.shadow_blur = blur * view.scale();
  }
  return edge;
}

void draw_marker(DrawList &list, const TownMapScript &script, const std::size_t index,
                 const bool chosen, const float pulse) {
  const TownView &view = script.view;
  const TownMarkerStyle &style = script.markers;
  const TownPlace &place = script.places[index];
  const auto spot = place.spot;
  const auto face_centre = shifted(spot, style.face_offset);
  const auto draw = [&](std::vector<ShapeOutline> outlines, const float alpha) {
    list.shape(style.layer, std::move(outlines), view.rect, view.units, alpha);
  };
  draw({ellipse(spot, style.radii, solid(style.rim)),
        ellipse(spot, inset(style.radii, style.bevel_inset), solid(style.bevel)),
        ellipse(face_centre, inset(style.radii, style.face_inset), solid(style.face))},
       1.0F);
  if (chosen) {
    draw({ellipse(spot, inset(style.radii, style.bevel_inset), solid(style.chosen_bevel)),
          ellipse(face_centre, inset(style.radii, style.face_inset), solid(style.chosen_face))},
         pulse);
  }
  // The number, leaning about its baseline, its ink centred on the disc.
  char number[8];
  std::snprintf(number, sizeof number, "%02d", static_cast<int>(index) + 1);
  const float baseline = spot[1] + style.number_baseline;
  const float middle =
      spot[0] + style.number_nudge - style.number_slant * style.number_height * 0.5F;
  const float half = style.radii[0];
  DisplayLayout digits;
  digits.centered = true;
  digits.capitals = true;
  digits.slant = style.number_slant;
  list.label(style.layer, number, style.number_font,
             view.box({middle - half, baseline - style.number_height, middle + half, baseline}),
             style.number_colour,
             edge_of(view, style.number_bold, style.number_colour, std::nullopt, 0.0F), digits);
  const Label label = label_of(style, place);
  draw(bar_code(style, place, label), 1.0F);
  DisplayLayout name;
  name.capitals = true;
  name.right = place.side < 0;
  TextShadow shadow;
  shadow.offset = style.shadow_offset;
  shadow.alpha = style.shadow_alpha;
  const float name_top = spot[1] + style.name_top;
  list.label(style.layer, place.name, style.name_font,
             view.box({label.left, name_top, label.right, name_top + style.name_height}),
             style.name_colour, edge_of(view, 0.0F, style.name_colour, shadow, style.shadow_blur),
             name);
  if (chosen) {
    draw({polygon(arrow(style, spot, -1.0F), solid(style.arrow)),
          polygon(arrow(style, spot, 1.0F), solid(style.arrow))},
         pulse);
  }
}

} // namespace

void draw_town_bands(DrawList &list, const TownMapScript &script) {
  for (const TownBand &band : script.bands) {
    list.shape(band.layer, band.outlines, script.view.rect, script.view.units);
  }
}

void draw_town_lettering(DrawList &list, const TownMapScript &script) {
  const TownView &view = script.view;
  for (const TownLettering &text : script.lettering) {
    DisplayLayout layout;
    layout.capitals = text.capitals;
    layout.right = text.right;
    layout.slant = text.slant;
    list.label(text.layer, text.text, text.font, view.box(text.box), text.colour,
               edge_of(view, text.bold, text.colour, text.shadow, text.shadow_blur), layout);
  }
}

// The shadows go down first, so that no marker's shadow falls on another.
void draw_town_markers(DrawList &list, const TownMapScript &script, const int chosen,
                       const float pulse) {
  const TownMarkerStyle &style = script.markers;
  std::vector<ShapeOutline> shadows;
  std::vector<ShapeOutline> bar_shadows;
  for (std::size_t index = 0; index < script.places.size(); ++index) {
    const TownPlace &place = script.places[index];
    const auto spot = shifted(place.spot, style.shadow_offset);
    shadows.push_back(ellipse(spot, style.radii, soft(style.shadow_alpha, style.shadow_blur)));
    const Label label = label_of(style, place);
    bar_shadows.push_back(
        rectangle({label.left + style.shadow_offset[0], spot[1] + style.bars_top,
                   label.right + style.shadow_offset[0], spot[1] + style.bars_bottom},
                  soft(style.bars_shadow_alpha, style.shadow_blur)));
    if (static_cast<int>(index) == chosen) {
      for (const float way : {-1.0F, 1.0F}) {
        auto points = arrow(style, place.spot, way);
        for (auto &point : points) {
          point = shifted(point, style.shadow_offset);
        }
        bar_shadows.push_back(
            polygon(std::move(points), soft(style.shadow_alpha * pulse, style.shadow_blur)));
      }
    }
  }
  list.shape(style.layer, std::move(shadows), script.view.rect, script.view.units);
  list.shape(style.layer, std::move(bar_shadows), script.view.rect, script.view.units);
  for (std::size_t index = 0; index < script.places.size(); ++index) {
    draw_marker(list, script, index, static_cast<int>(index) == chosen, pulse);
  }
}

} // namespace phantomcrash
