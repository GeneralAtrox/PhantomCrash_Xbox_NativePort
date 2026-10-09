#include "phantomcrash/sprite_shapes.hpp"

#include "phantomcrash/sprite_layer.hpp"

#include <algorithm>
#include <cmath>
#include <numbers>
#include <stdexcept>

namespace phantomcrash {

namespace {

template <std::size_t Size> std::array<float, Size> numbers(const json::JsonValue &value) {
  const auto &list = value.array();
  if (list.size() != Size) {
    throw std::runtime_error("sprite shape list needs " + std::to_string(Size) + " numbers");
  }
  std::array<float, Size> out{};
  for (std::size_t index = 0; index < Size; ++index) {
    out[index] = static_cast<float>(list[index].number());
  }
  return out;
}

// [distance, "#rrggbb", alpha]; alpha defaults to opaque.
ShapeStop parse_stop(const json::JsonValue &value) {
  const auto &list = value.array();
  if (list.size() < 2 || list.size() > 3) {
    throw std::runtime_error("sprite shape stop is [distance, colour, alpha]");
  }
  ShapeStop stop;
  stop.distance = static_cast<float>(list[0].number());
  stop.colour = parse_colour(list[1].string());
  stop.alpha =
      list.size() == 3 ? std::clamp(static_cast<float>(list[2].number()), 0.0F, 1.0F) : 1.0F;
  return stop;
}

ShapeOutline parse_outline(const json::JsonValue &value) {
  ShapeOutline outline;
  const auto *box = value.find("box");
  const auto *stops = value.find("stops");
  if (box == nullptr || stops == nullptr) {
    throw std::runtime_error("sprite shape outline needs a box and stops");
  }
  outline.box = numbers<4>(*box);
  outline.radius = std::max(0.0F, static_cast<float>(value.number_or("radius", 0.0)));
  outline.radius_growth = static_cast<float>(value.number_or("radius_growth", 1.0));
  for (const auto &stop : stops->array()) {
    outline.stops.push_back(parse_stop(stop));
  }
  if (outline.stops.empty()) {
    throw std::runtime_error("sprite shape outline needs stops");
  }
  std::stable_sort(outline.stops.begin(), outline.stops.end(),
                   [](const ShapeStop &a, const ShapeStop &b) { return a.distance < b.distance; });
  if (const auto *count = value.find("count")) {
    const auto cells = numbers<2>(*count);
    outline.count = {std::max(1, static_cast<int>(cells[0])),
                     std::max(1, static_cast<int>(cells[1]))};
  }
  if (const auto *step = value.find("step")) {
    outline.step = numbers<2>(*step);
  }
  return outline;
}

} // namespace

std::vector<SpriteShape> parse_sprite_shapes(const json::JsonValue &document) {
  std::vector<SpriteShape> shapes;
  const auto *list = document.find("shapes");
  if (list == nullptr) {
    return shapes;
  }
  for (const auto &entry : list->array()) {
    SpriteShape shape;
    shape.image = entry.string_or("image", "");
    const auto *rect = entry.find("rect");
    const auto *outlines = entry.find("outlines");
    if (shape.image.empty() || rect == nullptr || outlines == nullptr) {
      throw std::runtime_error("sprite shape needs an image, rect and outlines");
    }
    shape.rect = numbers<4>(*rect);
    for (const auto &outline : outlines->array()) {
      shape.outlines.push_back(parse_outline(outline));
    }
    shapes.push_back(std::move(shape));
  }
  return shapes;
}

std::vector<std::array<float, 2>> shape_contour(const ShapeOutline &outline,
                                                const std::array<float, 4> &box,
                                                const float distance, const int segments) {
  // The edge of a rounded rectangle moved by `distance` is one with its
  // radius grown by as much (with a growth of 1), square once the radius is
  // used up.
  const float centre_x = (box[0] + box[2]) * 0.5F;
  const float centre_y = (box[1] + box[3]) * 0.5F;
  const float half_x = std::max(0.0F, (box[2] - box[0]) * 0.5F + distance);
  const float half_y = std::max(0.0F, (box[3] - box[1]) * 0.5F + distance);
  const float round =
      std::clamp(outline.radius + outline.radius_growth * distance, 0.0F, std::min(half_x, half_y));
  // Corner centres clockwise from the top left, with each arc's start angle.
  const std::array<std::array<float, 3>, 4> corners{{
      {-1.0F, -1.0F, std::numbers::pi_v<float>},
      {1.0F, -1.0F, std::numbers::pi_v<float> * 1.5F},
      {1.0F, 1.0F, 0.0F},
      {-1.0F, 1.0F, std::numbers::pi_v<float> * 0.5F},
  }};
  const int steps = std::max(1, segments);
  std::vector<std::array<float, 2>> points;
  points.reserve(static_cast<std::size_t>(4 * (steps + 1)));
  for (const auto &[side_x, side_y, start] : corners) {
    const float corner_x = centre_x + side_x * (half_x - round);
    const float corner_y = centre_y + side_y * (half_y - round);
    for (int step = 0; step <= steps; ++step) {
      const float angle = start + std::numbers::pi_v<float> * 0.5F * static_cast<float>(step) /
                                      static_cast<float>(steps);
      points.push_back({corner_x + round * std::cos(angle), corner_y + round * std::sin(angle)});
    }
  }
  return points;
}

} // namespace phantomcrash
