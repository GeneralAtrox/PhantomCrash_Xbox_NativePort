#include "desktop_sprite_shapes.hpp"

#include <algorithm>
#include <cmath>
#include <exception>

namespace phantomcrash {

namespace {

// Corner segments: about one per two pixels of the largest radius.
constexpr float pixels_per_segment = 2.0F;
constexpr int max_segments = 24;

struct Mapping {
  float left{}, top{}, scale_x{}, scale_y{};
  [[nodiscard]] SDL_FPoint to_pixels(const std::array<float, 2> &point) const {
    return {left + point[0] * scale_x, top + point[1] * scale_y};
  }
};

SDL_FColor stop_colour(const ShapeStop &stop, const std::uint32_t tint, const float alpha) {
  const auto channel = [&](const int shift) {
    return static_cast<float>((stop.colour >> shift) & 0xffU) *
           static_cast<float>((tint >> shift) & 0xffU) / (255.0F * 255.0F);
  };
  return {channel(16), channel(8), channel(0), stop.alpha * alpha};
}

// The stops with each hard edge spread over `edge` texels around its
// distance, so it is antialiased over one screen pixel.
std::vector<ShapeStop> spread_edges(std::vector<ShapeStop> stops, const float edge) {
  for (std::size_t index = 0; index + 1 < stops.size(); ++index) {
    if (stops[index].distance == stops[index + 1].distance) {
      stops[index].distance -= edge * 0.5F;
      stops[index + 1].distance += edge * 0.5F;
    }
  }
  for (std::size_t index = 1; index < stops.size(); ++index) {
    stops[index].distance = std::max(stops[index].distance, stops[index - 1].distance);
  }
  return stops;
}

void draw_outline(SDL_Renderer *renderer, const ShapeOutline &outline,
                  const std::array<float, 4> &box, const Mapping &mapping, const std::uint32_t tint,
                  const float alpha) {
  const float texel = 0.5F * (mapping.scale_x + mapping.scale_y);
  const std::vector<ShapeStop> stops = spread_edges(outline.stops, 1.0F / texel);
  const float widest = outline.radius + std::max(0.0F, outline.radius_growth) *
                                            std::max(0.0F, stops.back().distance);
  const int segments =
      std::clamp(static_cast<int>(std::ceil(widest * texel / pixels_per_segment)), 1, max_segments);
  std::vector<SDL_Vertex> vertices;
  std::vector<int> indices;
  const auto ring = [&](const ShapeStop &stop) {
    const SDL_FColor colour = stop_colour(stop, tint, alpha);
    for (const auto &point : shape_contour(outline, box, stop.distance, segments)) {
      vertices.push_back({mapping.to_pixels(point), colour, {}});
    }
  };
  // The first stop's colour fills the inside, as a fan from the centre.
  const int count = 4 * (segments + 1);
  if (stops.front().alpha > 0.0F) {
    const std::array<float, 2> centre{(box[0] + box[2]) * 0.5F, (box[1] + box[3]) * 0.5F};
    vertices.push_back({mapping.to_pixels(centre), stop_colour(stops.front(), tint, alpha), {}});
    for (int point = 0; point < count; ++point) {
      indices.insert(indices.end(), {0, 1 + point, 1 + (point + 1) % count});
    }
  }
  const int first = static_cast<int>(vertices.size());
  for (const ShapeStop &stop : stops) {
    ring(stop);
  }
  for (std::size_t band = 0; band + 1 < stops.size(); ++band) {
    if (stops[band].alpha <= 0.0F && stops[band + 1].alpha <= 0.0F) {
      continue;
    }
    const int inner = first + static_cast<int>(band) * count;
    const int outer = inner + count;
    for (int point = 0; point < count; ++point) {
      const int next = (point + 1) % count;
      indices.insert(indices.end(), {inner + point, outer + point, outer + next, inner + point,
                                     outer + next, inner + next});
    }
  }
  if (!indices.empty()) {
    SDL_RenderGeometry(renderer, nullptr, vertices.data(), static_cast<int>(vertices.size()),
                       indices.data(), static_cast<int>(indices.size()));
  }
}

} // namespace

SpriteShapes::SpriteShapes(std::vector<SpriteShape> shapes) {
  for (SpriteShape &shape : shapes) {
    Key key{shape.image, shape.rect};
    shapes_.insert_or_assign(std::move(key), std::move(shape));
  }
}

const SpriteShape *SpriteShapes::find(const Sprite &sprite) const {
  const auto found = shapes_.find({sprite.image, sprite.rect});
  return found == shapes_.end() ? nullptr : &found->second;
}

void draw_sprite_shape(SDL_Renderer *renderer, const SpriteShape &shape, const SDL_FRect &target,
                       const std::uint32_t tint, const float alpha) {
  const float width = shape.rect[2] - shape.rect[0];
  const float height = shape.rect[3] - shape.rect[1];
  if (width <= 0.0F || height <= 0.0F || target.w < 0.5F || target.h < 0.5F) {
    return;
  }
  const Mapping mapping{target.x, target.y, target.w / width, target.h / height};
  SDL_BlendMode previous = SDL_BLENDMODE_NONE;
  SDL_GetRenderDrawBlendMode(renderer, &previous);
  SDL_SetRenderDrawBlendMode(renderer, SDL_BLENDMODE_BLEND);
  for (const ShapeOutline &outline : shape.outlines) {
    for (int row = 0; row < outline.count[1]; ++row) {
      for (int column = 0; column < outline.count[0]; ++column) {
        const float x = outline.step[0] * static_cast<float>(column);
        const float y = outline.step[1] * static_cast<float>(row);
        const std::array<float, 4> box{outline.box[0] + x, outline.box[1] + y, outline.box[2] + x,
                                       outline.box[3] + y};
        draw_outline(renderer, outline, box, mapping, tint, alpha);
      }
    }
  }
  SDL_SetRenderDrawBlendMode(renderer, previous);
}

std::unique_ptr<SpriteShapes> load_sprite_shapes(const DataRoots &roots) {
  try {
    return std::make_unique<SpriteShapes>(
        parse_sprite_shapes(roots.document("ui/sprite_shapes.json")));
  } catch (const std::exception &error) {
    SDL_Log("ui/sprite_shapes.json: %s", error.what());
    return nullptr;
  }
}

} // namespace phantomcrash
