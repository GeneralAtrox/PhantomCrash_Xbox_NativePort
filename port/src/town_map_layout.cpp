// The widescreen town map's view, markers, bands, lettering and water.

#include "phantomcrash/town_map_layout.hpp"

#include "phantomcrash/json_fields.hpp"

#include <algorithm>
#include <stdexcept>

namespace phantomcrash {
namespace {

using json::numbers;
using json::required;

float number(const json::JsonValue &value, const char *key, const float fallback) {
  return static_cast<float>(value.number_or(key, fallback));
}

std::uint32_t colour(const json::JsonValue &value, const char *key, const std::uint32_t fallback) {
  const auto *found = value.find(key);
  return found != nullptr ? parse_colour(found->string()) : fallback;
}

std::array<float, 2> pair(const json::JsonValue &value, const char *key,
                          const std::array<float, 2> &fallback) {
  const auto *found = value.find(key);
  return found != nullptr ? numbers<2>(*found, key) : fallback;
}

} // namespace

TownMarkerBoxes town_marker_boxes(const TownView &view, const TownMarkerStyle &style,
                                  const std::array<float, 2> &spot, const int side,
                                  const float label_width) {
  const auto [x, y] = spot;
  const auto [rx, ry] = style.radii;
  const float width = std::max(label_width, style.min_label_width);
  const float left = side > 0 ? x + rx + style.label_gap : x - rx - style.label_gap - width;
  const float blur = style.shadow_blur;
  const auto [drop_x, drop_y] = style.shadow_offset;
  TownMarkerBoxes boxes;
  boxes.centre = view.point(spot);
  boxes.radii = {rx * view.scale(), ry * view.scale()};
  boxes.disc = view.box({x - rx, y - ry, x + rx, y + ry});
  boxes.shadowed_disc =
      view.box({x - rx - blur, y - ry - blur, x + rx + drop_x + blur, y + ry + drop_y + blur});
  boxes.bars = view.box({left, y + style.bars_top, left + width, y + style.bars_bottom});
  boxes.label = view.box(
      {left, y + style.bars_top, left + width, y + style.name_top + style.name_height + drop_y});
  return boxes;
}

bool on_water(const TownWater &water, const Sprite &map, const std::array<float, 2> &point) {
  const float width = map.rect[2] - map.rect[0];
  const float height = map.rect[3] - map.rect[1];
  const auto size = map.size.value_or(std::array<float, 2>{width, height});
  if (size[0] <= 0.0F || size[1] <= 0.0F) {
    return false;
  }
  const float x = map.rect[0] + (point[0] - map.position[0]) * width / size[0];
  const float y = map.rect[1] + (point[1] - map.position[1]) * height / size[1];
  for (const auto &polygon : water.polygons) {
    bool inside = false;
    for (std::size_t index = 0, last = polygon.size() - 1; index < polygon.size(); last = index++) {
      const auto &a = polygon[index];
      const auto &b = polygon[last];
      if ((a[1] > y) != (b[1] > y) && x < (b[0] - a[0]) * (y - a[1]) / (b[1] - a[1]) + a[0]) {
        inside = !inside;
      }
    }
    if (inside) {
      return true;
    }
  }
  return false;
}

TownView parse_town_view(const json::JsonValue &value) {
  TownView view;
  view.rect = numbers<4>(required(value, "rect"), "rect");
  view.units = numbers<2>(required(value, "units"), "units");
  if (view.rect[2] <= view.rect[0] || view.rect[3] <= view.rect[1] || view.units[0] <= 0.0F ||
      view.units[1] <= 0.0F) {
    throw std::runtime_error("view needs a rect and units with some size");
  }
  return view;
}

TownMarkerStyle parse_town_markers(const json::JsonValue &value) {
  TownMarkerStyle style;
  style.radii = pair(value, "radii", style.radii);
  style.bevel_inset = number(value, "bevel_inset", style.bevel_inset);
  style.face_inset = number(value, "face_inset", style.face_inset);
  style.face_offset = pair(value, "face_offset", style.face_offset);
  style.rim = colour(value, "rim", style.rim);
  style.bevel = colour(value, "bevel", style.bevel);
  style.face = colour(value, "face", style.face);
  style.chosen_bevel = colour(value, "chosen_bevel", style.chosen_bevel);
  style.chosen_face = colour(value, "chosen_face", style.chosen_face);
  style.shadow_offset = pair(value, "shadow_offset", style.shadow_offset);
  style.shadow_alpha = number(value, "shadow_alpha", style.shadow_alpha);
  style.shadow_blur = number(value, "shadow_blur", style.shadow_blur);
  style.number_font = value.string_or("number_font", style.number_font);
  style.number_colour = colour(value, "number_colour", style.number_colour);
  style.number_height = number(value, "number_height", style.number_height);
  style.number_baseline = number(value, "number_baseline", style.number_baseline);
  style.number_nudge = number(value, "number_nudge", style.number_nudge);
  style.number_slant = number(value, "number_slant", style.number_slant);
  style.number_bold = number(value, "number_bold", style.number_bold);
  style.arrow = colour(value, "arrow", style.arrow);
  style.arrow_gap = number(value, "arrow_gap", style.arrow_gap);
  style.arrow_reach = number(value, "arrow_reach", style.arrow_reach);
  style.arrow_half = number(value, "arrow_half", style.arrow_half);
  style.label_gap = number(value, "label_gap", style.label_gap);
  style.min_label_width = number(value, "min_label_width", style.min_label_width);
  style.bars_top = number(value, "bars_top", style.bars_top);
  style.bars_bottom = number(value, "bars_bottom", style.bars_bottom);
  style.bar_trim = number(value, "bar_trim", style.bar_trim);
  style.bars_shadow_alpha = number(value, "bars_shadow_alpha", style.bars_shadow_alpha);
  style.name_font = value.string_or("name_font", style.name_font);
  style.name_colour = colour(value, "name_colour", style.name_colour);
  style.name_top = number(value, "name_top", style.name_top);
  style.name_height = number(value, "name_height", style.name_height);
  style.layer = static_cast<int>(value.number_or("layer", style.layer));
  if (style.radii[0] <= 0.0F || style.radii[1] <= 0.0F) {
    throw std::runtime_error("markers: radii must be above 0");
  }
  return style;
}

std::vector<TownBand> parse_town_bands(const json::JsonValue &list) {
  std::vector<TownBand> bands;
  for (const auto &value : list.array()) {
    TownBand band;
    band.layer = static_cast<int>(value.number_or("layer", band.layer));
    band.outlines = parse_shape_outlines(required(value, "outlines"));
    bands.push_back(std::move(band));
  }
  return bands;
}

std::vector<TownLettering> parse_town_lettering(const json::JsonValue &list) {
  std::vector<TownLettering> lettering;
  for (const auto &value : list.array()) {
    TownLettering text;
    text.text = required(value, "text").string();
    text.font = required(value, "font").string();
    text.box = numbers<4>(required(value, "box"), "box");
    text.colour = colour(value, "colour", text.colour);
    text.slant = number(value, "slant", text.slant);
    text.bold = number(value, "bold", text.bold);
    text.capitals = value.boolean_or("capitals", text.capitals);
    text.right = value.boolean_or("right", text.right);
    if (const auto *shadow = value.find("shadow")) {
      TextShadow drop;
      drop.offset = pair(*shadow, "offset", drop.offset);
      drop.colour = colour(*shadow, "colour", drop.colour);
      drop.alpha = number(*shadow, "alpha", 1.0F);
      text.shadow = drop;
      text.shadow_blur = number(*shadow, "blur", 0.0F);
    }
    text.layer = static_cast<int>(value.number_or("layer", text.layer));
    lettering.push_back(std::move(text));
  }
  return lettering;
}

TownWater parse_town_water(const json::JsonValue &value) {
  TownWater water;
  water.map = required(value, "map").string();
  for (const auto &polygon : required(value, "polygons").array()) {
    std::vector<std::array<float, 2>> points;
    for (const auto &point : polygon.array()) {
      points.push_back(numbers<2>(point, "water point"));
    }
    if (points.size() < 3) {
      throw std::runtime_error("water: each polygon needs 3 points or more");
    }
    water.polygons.push_back(std::move(points));
  }
  return water;
}

} // namespace phantomcrash
