#include "phantomcrash/polygon_fill.hpp"

#include <algorithm>
#include <cmath>
#include <cstddef>
#include <limits>
#include <numeric>

namespace phantomcrash {

namespace {

using Point = std::array<float, 2>;

float cross(const Point &a, const Point &b, const Point &c) {
  return (b[0] - a[0]) * (c[1] - a[1]) - (b[1] - a[1]) * (c[0] - a[0]);
}

float signed_area(const std::vector<Point> &points, const std::vector<int> &ring) {
  float area = 0.0F;
  for (std::size_t index = 0; index < ring.size(); ++index) {
    const Point &a = points[static_cast<std::size_t>(ring[index])];
    const Point &b = points[static_cast<std::size_t>(ring[(index + 1) % ring.size()])];
    area += a[0] * b[1] - b[0] * a[1];
  }
  return area * 0.5F;
}

bool same(const Point &a, const Point &b) {
  return std::abs(a[0] - b[0]) < 1e-5F && std::abs(a[1] - b[1]) < 1e-5F;
}

// Whether segments ab and cd cross at a point inside both.
bool crosses(const Point &a, const Point &b, const Point &c, const Point &d) {
  const float d1 = cross(a, b, c), d2 = cross(a, b, d);
  const float d3 = cross(c, d, a), d4 = cross(c, d, b);
  return ((d1 > 0.0F && d2 < 0.0F) || (d1 < 0.0F && d2 > 0.0F)) &&
         ((d3 > 0.0F && d4 < 0.0F) || (d3 < 0.0F && d4 > 0.0F));
}

bool blocked(const std::vector<Point> &points, const std::vector<std::vector<int>> &rings,
             const Point &from, const Point &to) {
  for (const auto &ring : rings) {
    for (std::size_t index = 0; index < ring.size(); ++index) {
      const Point &a = points[static_cast<std::size_t>(ring[index])];
      const Point &b = points[static_cast<std::size_t>(ring[(index + 1) % ring.size()])];
      if (!same(a, from) && !same(a, to) && !same(b, from) && !same(b, to) &&
          crosses(from, to, a, b)) {
        return true;
      }
    }
  }
  return false;
}

// Joins `hole` into `outer` through its rightmost vertex and the nearest
// vertex of the outer ring that it can see without crossing an edge.
void bridge(const std::vector<Point> &points, std::vector<int> &outer, const std::vector<int> &hole,
            const std::vector<std::vector<int>> &others) {
  const auto rightmost = std::max_element(hole.begin(), hole.end(), [&](const int a, const int b) {
    return points[static_cast<std::size_t>(a)][0] < points[static_cast<std::size_t>(b)][0];
  });
  const Point &from = points[static_cast<std::size_t>(*rightmost)];
  std::vector<std::vector<int>> rings = others;
  rings.push_back(outer);
  std::size_t best = 0;
  float nearest = std::numeric_limits<float>::max();
  for (std::size_t index = 0; index < outer.size(); ++index) {
    const Point &to = points[static_cast<std::size_t>(outer[index])];
    const float distance = std::hypot(to[0] - from[0], to[1] - from[1]);
    if (distance < nearest && !blocked(points, rings, from, to)) {
      nearest = distance;
      best = index;
    }
  }
  std::vector<int> joined(outer.begin(), outer.begin() + static_cast<std::ptrdiff_t>(best) + 1);
  const auto start = static_cast<std::size_t>(rightmost - hole.begin());
  for (std::size_t step = 0; step <= hole.size(); ++step) {
    joined.push_back(hole[(start + step) % hole.size()]);
  }
  joined.insert(joined.end(), outer.begin() + static_cast<std::ptrdiff_t>(best), outer.end());
  outer = std::move(joined);
}

bool inside_triangle(const Point &p, const Point &a, const Point &b, const Point &c) {
  return cross(a, b, p) >= 0.0F && cross(b, c, p) >= 0.0F && cross(c, a, p) >= 0.0F;
}

} // namespace

std::vector<int> triangulate_polygon(const std::vector<PolygonContour> &contours) {
  std::vector<Point> points;
  std::vector<std::vector<int>> rings;
  for (const auto &contour : contours) {
    std::vector<int> ring(contour.size());
    std::iota(ring.begin(), ring.end(), static_cast<int>(points.size()));
    points.insert(points.end(), contour.begin(), contour.end());
    if (ring.size() >= 3) {
      rings.push_back(std::move(ring));
    }
  }
  std::vector<int> triangles;
  if (rings.empty()) {
    return triangles;
  }
  // The outer ring counter-clockwise (in these axes), the holes the other way.
  for (std::size_t index = 0; index < rings.size(); ++index) {
    const bool positive = signed_area(points, rings[index]) > 0.0F;
    if (positive != (index == 0)) {
      std::reverse(rings[index].begin(), rings[index].end());
    }
  }
  std::vector<int> ring = rings.front();
  std::vector<std::vector<int>> holes(rings.begin() + 1, rings.end());
  std::sort(holes.begin(), holes.end(), [&](const auto &a, const auto &b) {
    const auto right = [&](const std::vector<int> &hole) {
      float x = -std::numeric_limits<float>::max();
      for (const int index : hole) {
        x = std::max(x, points[static_cast<std::size_t>(index)][0]);
      }
      return x;
    };
    return right(a) > right(b);
  });
  for (std::size_t index = 0; index < holes.size(); ++index) {
    const std::vector<std::vector<int>> rest(holes.begin() + static_cast<std::ptrdiff_t>(index),
                                             holes.end());
    bridge(points, ring, holes[index], rest);
  }
  const auto at = [&](const int index) -> const Point & {
    return points[static_cast<std::size_t>(index)];
  };
  // Clip ears; a ring that has none left (a fold) loses its first vertex.
  while (ring.size() > 3) {
    const std::size_t count = ring.size();
    std::size_t ear = count;
    for (std::size_t index = 0; index < count && ear == count; ++index) {
      const Point &a = at(ring[(index + count - 1) % count]);
      const Point &b = at(ring[index]);
      const Point &c = at(ring[(index + 1) % count]);
      if (cross(a, b, c) <= 0.0F) {
        continue;
      }
      bool empty = true;
      for (std::size_t other = 0; other < count && empty; ++other) {
        const Point &p = at(ring[other]);
        empty = same(p, a) || same(p, b) || same(p, c) || !inside_triangle(p, a, b, c);
      }
      if (empty) {
        ear = index;
      }
    }
    if (ear == count) {
      ear = 0;
    }
    triangles.insert(triangles.end(),
                     {ring[(ear + count - 1) % count], ring[ear], ring[(ear + 1) % count]});
    ring.erase(ring.begin() + static_cast<std::ptrdiff_t>(ear));
  }
  triangles.insert(triangles.end(), ring.begin(), ring.end());
  return triangles;
}

} // namespace phantomcrash
