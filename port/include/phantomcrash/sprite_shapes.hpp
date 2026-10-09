#pragma once

#include "phantomcrash/json.hpp"

#include <array>
#include <cstdint>
#include <string>
#include <vector>

namespace phantomcrash {

// One colour of an outline's profile, `distance` texels from its box's edge
// (negative inside). Two stops at one distance make a hard edge, drawn one
// screen pixel wide; inside the first stop its colour fills the box, and
// outside the last nothing is drawn.
struct ShapeStop {
  float distance{};
  std::uint32_t colour{0xffffff};
  float alpha{1.0F};
};

// A rounded rectangle (rect-relative LTRB, in the sprite's texels) coloured
// by distance from its edge, repeated over `count` cells `step` apart. Its
// corners' radius grows by `radius_growth` times the distance (1 keeps the
// edges parallel; the art's rings are often rounder inside than outside).
struct ShapeOutline {
  std::array<float, 4> box{};
  float radius{};
  float radius_growth{1.0F};
  std::vector<ShapeStop> stops;
  std::array<int, 2> count{1, 1};
  std::array<float, 2> step{};
};

// data/ui/sprite_shapes.json: plain frames and cursors the menus draw from
// their atlases, redrawn as outlines at the output resolution. Found by the
// sprite's image and rect.
struct SpriteShape {
  std::string image;
  std::array<float, 4> rect{};
  std::vector<ShapeOutline> outlines;
};

[[nodiscard]] std::vector<SpriteShape> parse_sprite_shapes(const json::JsonValue &document);

// The outline's edge moved `distance` texels outward (inward when
// negative): `segments` + 1 points per corner, clockwise from the top-left
// corner's left end, the same count at every distance so contours pair up.
[[nodiscard]] std::vector<std::array<float, 2>> shape_contour(const ShapeOutline &outline,
                                                              const std::array<float, 4> &box,
                                                              float distance, int segments);

} // namespace phantomcrash
