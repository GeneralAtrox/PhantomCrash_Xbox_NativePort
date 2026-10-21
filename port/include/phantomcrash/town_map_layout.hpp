#pragma once

#include "phantomcrash/json.hpp"
#include "phantomcrash/sprite_layer.hpp"
#include "phantomcrash/sprite_shapes.hpp"
#include "phantomcrash/sprite_text.hpp"

#include <array>
#include <cstdint>
#include <optional>
#include <string>
#include <vector>

namespace phantomcrash {

// The widescreen town map's own drawing (data/ui/town_map.json "view",
// "markers", "bands", "lettering" and "water"), a port addition: the map's
// picture at exactly twice its size across a 16:9 view, the places' markers
// and the bands drawn as vector art over it. Its geometry is in view units:
// the view as 1280x720, so one map texel is two units.
struct TownView {
  std::array<float, 4> rect{-320.0F / 3.0F, 0.0F, 640.0F + 320.0F / 3.0F, 480.0F}; // canvas
  std::array<float, 2> units{1280.0F, 720.0F};

  [[nodiscard]] float scale() const {
    return (rect[2] - rect[0]) / units[0];
  }
  [[nodiscard]] std::array<float, 2> point(const std::array<float, 2> &at) const {
    return {rect[0] + at[0] * scale(), rect[1] + at[1] * (rect[3] - rect[1]) / units[1]};
  }
  [[nodiscard]] std::array<float, 4> box(const std::array<float, 4> &at) const {
    const auto [left, top] = point({at[0], at[1]});
    const auto [right, bottom] = point({at[2], at[3]});
    return {left, top, right, bottom};
  }
};

// Display lettering in a data/ui/sprite_text.json font: its ink's box (view
// units; for "capitals", the capitals' box), leaning by `slant` (x per y),
// emboldened by `bold` (view units of outline in its own colour) and with
// an optional soft shadow (offset and blur in view units).
struct TownLettering {
  std::string text;
  std::string font;
  std::array<float, 4> box{};
  std::uint32_t colour{0xffffff};
  float slant{};
  float bold{};
  bool capitals{true};
  bool right{};
  std::optional<TextShadow> shadow;
  float shadow_blur{};
  int layer{2};
};

// Each place's marker: a disc with its number, a bar code and its name.
// The disc is an ellipse of `radii` with a black rim, an inner bevel and the
// face (`face_offset` from the middle, `face_inset` in from the rim's edge);
// the chosen place's face turns yellow, pulsing, with red arrows either side.
// The number is set with its capitals' height `number_height` and its
// baseline `number_baseline` below the disc's middle. The bar code stands
// `label_gap` beside the disc, from `bars_top` to `bars_bottom` (from the
// disc's middle), as wide as the name or `min_label_width`; the name is set
// below it, its capitals from `name_top` down `name_height`. The disc, the
// name and the arrows cast a soft shadow (`shadow_offset`, `shadow_blur`).
struct TownMarkerStyle {
  std::array<float, 2> radii{27.0F, 21.0F};
  float bevel_inset{3.4F};
  float face_inset{4.6F};
  std::array<float, 2> face_offset{-0.8F, -0.9F};
  std::uint32_t rim{0x000000};
  std::uint32_t bevel{0xc4c4be};
  std::uint32_t face{0xffffff};
  std::uint32_t chosen_bevel{0xceb200};
  std::uint32_t chosen_face{0xffee00};
  std::array<float, 2> shadow_offset{3.0F, 4.0F};
  float shadow_alpha{0.55F};
  float shadow_blur{2.2F};
  std::string number_font{"town_number"};
  std::uint32_t number_colour{0x000000};
  float number_height{23.75F};
  float number_baseline{11.5F};
  float number_nudge{0.5F}; // the number sits this far right of the middle
  float number_slant{0.21F};
  float number_bold{0.7F};
  std::uint32_t arrow{0xe80000};
  float arrow_gap{3.0F};    // the arrows' bases stand this far out from the disc
  float arrow_reach{10.0F}; // and their tips this much further
  float arrow_half{9.0F};   // half their bases' height
  float label_gap{17.0F};
  float min_label_width{64.0F};
  float bars_top{-16.0F};
  float bars_bottom{7.0F};
  float bar_trim{0.1F};          // each bar ends this short of the next space
  float bars_shadow_alpha{0.3F}; // the bar code's shadow, one soft block under it
  std::string name_font{"town_label"};
  std::uint32_t name_colour{0x000000};
  float name_top{10.0F};
  float name_height{8.25F};
  int layer{257};
};

// A part of the bands: vector outlines over the view, in view units.
struct TownBand {
  int layer{2};
  std::vector<ShapeOutline> outlines;
};

// Where the places' discs and labels stand, in canvas units, and the boxes
// the layout check keeps clear of each other.
struct TownMarkerBoxes {
  std::array<float, 2> centre{};
  std::array<float, 2> radii{};
  std::array<float, 4> disc{};  // the ellipse's bounds
  std::array<float, 4> label{}; // the bar code and name
  std::array<float, 4> bars{};
  std::array<float, 4> shadowed_disc{}; // with its shadow
};

// The places' markers seen as hit areas and their layout (all canvas
// units): `spot` is the disc's middle, `side` the side its label goes
// (-1 left, 1 right) and `label_width` its name's width.
[[nodiscard]] TownMarkerBoxes town_marker_boxes(const TownView &view, const TownMarkerStyle &style,
                                                const std::array<float, 2> &spot, int side,
                                                float label_width);

// The water under the map (the bay and its pools) as polygons in the map
// picture's texels, and the map's placement, which turns them into canvas
// units. The places stand on land: their discs and labels avoid it.
struct TownWater {
  std::string map; // the layer id of the map picture
  std::vector<std::vector<std::array<float, 2>>> polygons;
};

// Whether the canvas point is on water, the map being `map`.
[[nodiscard]] bool on_water(const TownWater &water, const Sprite &map,
                            const std::array<float, 2> &point);

// Throw std::runtime_error naming the entry at fault.
[[nodiscard]] TownView parse_town_view(const json::JsonValue &value);
[[nodiscard]] TownMarkerStyle parse_town_markers(const json::JsonValue &value);
[[nodiscard]] std::vector<TownBand> parse_town_bands(const json::JsonValue &list);
[[nodiscard]] std::vector<TownLettering> parse_town_lettering(const json::JsonValue &list);
[[nodiscard]] TownWater parse_town_water(const json::JsonValue &value);

} // namespace phantomcrash
