#pragma once

#include <algorithm>

namespace phantomcrash {

// The original draws in a 640x480 space. The port keeps those coordinates and
// fits them to the window at full resolution: 480 units always span the
// height of a widescreen window, so the 4:3 area sits centred and the extra
// width to either side (negative x, and x beyond 640) is visible too. Narrower
// windows letterbox instead.
inline constexpr float canvas_width = 640.0F;
inline constexpr float canvas_height = 480.0F;

struct PixelRect {
  float x{}, y{}, width{}, height{};
};

struct Viewport {
  float scale{1.0F};
  // Pixel position of canvas (0, 0).
  float offset_x{}, offset_y{};
  int pixel_width{}, pixel_height{};

  [[nodiscard]] float to_pixel_x(float x) const {
    return offset_x + x * scale;
  }
  [[nodiscard]] float to_pixel_y(float y) const {
    return offset_y + y * scale;
  }
  [[nodiscard]] PixelRect to_pixels(float x, float y, float width, float height) const {
    return {to_pixel_x(x), to_pixel_y(y), width * scale, height * scale};
  }
  // The 4:3 area, where movies and the original screens are drawn.
  [[nodiscard]] PixelRect content() const {
    return to_pixels(0.0F, 0.0F, canvas_width, canvas_height);
  }
  // Canvas x of the window's left and right edges (-ve / beyond 640 when wide).
  [[nodiscard]] float visible_left() const {
    return -offset_x / scale;
  }
  [[nodiscard]] float visible_right() const {
    return (static_cast<float>(pixel_width) - offset_x) / scale;
  }
};

[[nodiscard]] inline Viewport fit_canvas(int pixel_width, int pixel_height) {
  Viewport viewport;
  viewport.pixel_width = std::max(pixel_width, 1);
  viewport.pixel_height = std::max(pixel_height, 1);
  const float width = static_cast<float>(viewport.pixel_width);
  const float height = static_cast<float>(viewport.pixel_height);
  viewport.scale = std::min(width / canvas_width, height / canvas_height);
  viewport.offset_x = (width - canvas_width * viewport.scale) * 0.5F;
  viewport.offset_y = (height - canvas_height * viewport.scale) * 0.5F;
  return viewport;
}

} // namespace phantomcrash
