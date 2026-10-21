#pragma once

#include <array>
#include <vector>

namespace phantomcrash {

using PolygonContour = std::vector<std::array<float, 2>>;

// Triangles filling a polygon with holes: `contours` is the outer contour
// followed by the holes inside it, in either winding. Each hole is joined to
// the outer contour by a cut to a vertex it can see, then the whole is
// clipped ear by ear. The indices count through the contours' points in
// order (the outer contour's first), three to a triangle, so the same
// indices fill any contours moved slightly from these.
[[nodiscard]] std::vector<int> triangulate_polygon(const std::vector<PolygonContour> &contours);

} // namespace phantomcrash
