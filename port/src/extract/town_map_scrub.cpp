// The town map picture with its painted markers and title inpainted away.
#include "town_map_scrub.hpp"

#include "png_write.hpp"
#include "town_map_mask.hpp"
#include "xbox_texture.hpp"

#include <algorithm>
#include <array>
#include <cmath>
#include <limits>
#include <stdexcept>
#include <vector>

namespace phantomcrash::extract {
namespace {

using namespace town_mask;

constexpr int W = town_map_width;
constexpr int H = town_map_height;
constexpr const char *town_map_pack = "scroll/town_map_tex.bin";

// The painted markers (each place's number, bar code and name), as the
// town map's places' hit areas give them, and the TOWN MAP title: left,
// top, right, bottom.
constexpr std::array<std::array<int, 4>, 8> markers{{{300, 72, 412, 118},
                                                     {206, 182, 358, 236},
                                                     {90, 110, 250, 156},
                                                     {52, 240, 214, 302},
                                                     {312, 290, 486, 360},
                                                     {434, 222, 582, 284},
                                                     {462, 162, 590, 216},
                                                     {416, 104, 592, 148}}};
constexpr std::array<int, 4> title{40, 358, 392, 416};
// A marker's box grows by this much before and after (its shadow falls
// down and right).
constexpr int margin_before = 3;
constexpr int margin_after = 6;
constexpr int ink_chroma = 70;  // marker ink is grey: white discs, black bars and letters
constexpr int closing = 2;      // the ink's gaps closed over this many texels
constexpr int marker_grow = 4;  // the hole grows round the ink and its shadow
constexpr int title_shadow = 4; // the title's shadow spreads this far
constexpr int title_grow = 3;
constexpr std::array<int, 2> marker_shadow{4, 4}; // the painted shadow's offset (x, y)
constexpr std::array<int, 2> title_offset{5, 4};
constexpr int patch = 5;            // half a patch: 11x11
constexpr int scanline = 6;         // the picture's scanline period: sources keep the row phase
constexpr int search = 90;          // the farthest a source patch may be
constexpr double nearness = 0.002;  // a source's cost grows by this share per texel away
constexpr double data_floor = 0.05; // a front point's priority with no edge reaching it

// The texels to fill: each marker's and the title's grey ink, closed and
// filled, with the soft shadow down and right of it.
Mask hole_of(const std::vector<float> &image) {
  Mask ink(static_cast<std::size_t>(W) * H, 0);
  for (std::size_t index = 0; index < ink.size(); ++index) {
    const float *texel = &image[index * 3];
    const float chroma =
        std::max({texel[0], texel[1], texel[2]}) - std::min({texel[0], texel[1], texel[2]});
    ink[index] = chroma < ink_chroma ? 1 : 0;
  }
  const auto closed = [&](const std::array<int, 4> &box) {
    return fill_holes(erode(dilate(within(ink, box), closing), closing));
  };
  Mask hole(ink.size(), 0);
  for (const auto &[x0, y0, x1, y1] : markers) {
    const std::array<int, 4> box{x0 - margin_before, y0 - margin_before, x1 + margin_after,
                                 y1 + margin_after};
    const Mask marker = closed(box);
    const Mask grown = dilate(either(marker, shifted(marker, marker_shadow)), marker_grow);
    hole = either(hole, within(grown, box));
  }
  const Mask lettering = closed(title);
  const Mask shadow = shifted(dilate(lettering, title_shadow, true), title_offset);
  return either(hole, within(dilate(either(lettering, shadow), title_grow), title));
}

struct Inpainter {
  std::vector<float> image; // RGB
  // The picture's gray as painted, markers and all: the edges running into
  // the hole are measured on it, so the fill follows the marker's outline
  // inward rather than lines in the texels just copied.
  std::vector<float> painted;
  Mask known;
  std::vector<float> confidence;
  Mask forbidden; // centres whose patch would reach the hole

  [[nodiscard]] float gray(const int x, const int y) const {
    return painted[at(x, y)];
  }
  [[nodiscard]] bool is_known(const int x, const int y) const {
    return inside(x, y) && known[at(x, y)] != 0;
  }
  // The gray gradient at a known texel, from its known neighbours.
  [[nodiscard]] std::array<float, 2> gradient(const int x, const int y) const {
    const auto along = [&](const int dx, const int dy) {
      const bool ahead = is_known(x + dx, y + dy), behind = is_known(x - dx, y - dy);
      if (ahead && behind) {
        return (gray(x + dx, y + dy) - gray(x - dx, y - dy)) * 0.5F;
      }
      if (ahead) {
        return gray(x + dx, y + dy) - gray(x, y);
      }
      if (behind) {
        return gray(x, y) - gray(x - dx, y - dy);
      }
      return 0.0F;
    };
    return {along(1, 0), along(0, 1)};
  }
  // The front point's priority: how much of its patch is trusted, times how
  // strongly an edge runs into the hole there.
  [[nodiscard]] double priority(const int x, const int y) const {
    double trusted = 0.0;
    for (int dy = -patch; dy <= patch; ++dy) {
      for (int dx = -patch; dx <= patch; ++dx) {
        if (inside(x + dx, y + dy)) {
          trusted += confidence[at(x + dx, y + dy)];
        }
      }
    }
    trusted /= (2 * patch + 1) * (2 * patch + 1);
    const auto filled = [&](const int px, const int py) { return is_known(px, py) ? 1.0F : 0.0F; };
    const float nx = (filled(x + 1, y) - filled(x - 1, y)) * 0.5F;
    const float ny = (filled(x, y + 1) - filled(x, y - 1)) * 0.5F;
    const float length = std::hypot(nx, ny) + 1e-6F;
    float gx = 0.0F, gy = 0.0F;
    for (int dy = -1; dy <= 1; ++dy) {
      for (int dx = -1; dx <= 1; ++dx) {
        if (is_known(x + dx, y + dy)) {
          const auto [ax, ay] = gradient(x + dx, y + dy);
          gx = std::max(gx, std::abs(ax));
          gy = std::max(gy, std::abs(ay));
        }
      }
    }
    const double edge = std::abs(-gy * nx / length + gx * ny / length) / 255.0;
    return trusted * (data_floor + edge);
  }
  // The unknown texel with a known neighbour that goes first.
  [[nodiscard]] std::array<int, 2> next() const {
    double best = -1.0;
    std::array<int, 2> chosen{-1, -1};
    for (int y = 0; y < H; ++y) {
      for (int x = 0; x < W; ++x) {
        if (known[at(x, y)] != 0 || !(is_known(x + 1, y) || is_known(x - 1, y) ||
                                      is_known(x, y + 1) || is_known(x, y - 1))) {
          continue;
        }
        if (const double value = priority(x, y); value > best) {
          best = value;
          chosen = {x, y};
        }
      }
    }
    return chosen;
  }
  // The intact patch nearby, on the same scanline phase, most like the
  // known part of the patch centred at (cx, cy).
  [[nodiscard]] std::array<int, 2> source(const int cx, const int cy) const {
    double best = std::numeric_limits<double>::infinity();
    std::array<int, 2> found{-1, -1};
    const int top = std::max(patch, cy - search), bottom = std::min(H - 1 - patch, cy + search);
    const int left = std::max(patch, cx - search), right = std::min(W - 1 - patch, cx + search);
    for (int sy = top; sy <= bottom; ++sy) {
      if ((sy - cy) % scanline != 0) {
        continue;
      }
      for (int sx = left; sx <= right; ++sx) {
        if (forbidden[at(sx, sy)] != 0) {
          continue;
        }
        const double weight = 1.0 + nearness * std::hypot(sx - cx, sy - cy);
        const double limit = best / weight;
        double distance = 0.0;
        for (int dy = -patch; dy <= patch && distance < limit; ++dy) {
          for (int dx = -patch; dx <= patch; ++dx) {
            if (known[at(cx + dx, cy + dy)] == 0) {
              continue;
            }
            const float *a = &image[at(cx + dx, cy + dy) * 3];
            const float *b = &image[at(sx + dx, sy + dy) * 3];
            for (int channel = 0; channel < 3; ++channel) {
              const double difference = a[channel] - b[channel];
              distance += difference * difference;
            }
          }
        }
        if (distance * weight < best) {
          best = distance * weight;
          found = {sx, sy};
        }
      }
    }
    return found;
  }
  void run() {
    while (true) {
      const auto [px, py] = next();
      if (px < 0) {
        return;
      }
      const int cx = std::clamp(px, patch, W - 1 - patch);
      const int cy = std::clamp(py, patch, H - 1 - patch);
      const auto [sx, sy] = source(cx, cy);
      if (sx < 0) {
        throw std::runtime_error("town map scrub: no source patch");
      }
      double trusted = 0.0;
      for (int dy = -patch; dy <= patch; ++dy) {
        for (int dx = -patch; dx <= patch; ++dx) {
          trusted += confidence[at(cx + dx, cy + dy)];
        }
      }
      // Filled texels are trusted far less than painted ones, so the hole
      // closes evenly from its edge rather than along one copied line.
      trusted /= std::pow((2 * patch + 1) * (2 * patch + 1), 2);
      for (int dy = -patch; dy <= patch; ++dy) {
        for (int dx = -patch; dx <= patch; ++dx) {
          const std::size_t target = at(cx + dx, cy + dy);
          if (known[target] != 0) {
            continue;
          }
          std::copy_n(&image[at(sx + dx, sy + dy) * 3], 3, &image[target * 3]);
          confidence[target] = static_cast<float>(trusted);
          known[target] = 1;
        }
      }
    }
  }
};

} // namespace

Bytes scrub_town_map(const int width, const int height, const ByteView rgba) {
  if (width < W || height < H || rgba.size() < static_cast<std::size_t>(width) * height * 4) {
    throw std::runtime_error("town map scrub: the picture is smaller than the map");
  }
  Inpainter paint;
  paint.image.resize(static_cast<std::size_t>(W) * H * 3);
  for (int y = 0; y < H; ++y) {
    for (int x = 0; x < W; ++x) {
      const std::size_t from = (static_cast<std::size_t>(y) * width + x) * 4;
      for (int channel = 0; channel < 3; ++channel) {
        paint.image[at(x, y) * 3 + channel] = rgba[from + channel];
      }
    }
  }
  paint.painted.resize(static_cast<std::size_t>(W) * H);
  for (std::size_t index = 0; index < paint.painted.size(); ++index) {
    const float *texel = &paint.image[index * 3];
    paint.painted[index] = (texel[0] + texel[1] + texel[2]) / 3.0F;
  }
  const Mask hole = hole_of(paint.image);
  paint.known.resize(hole.size());
  paint.confidence.resize(hole.size());
  for (std::size_t index = 0; index < hole.size(); ++index) {
    paint.known[index] = hole[index] == 0 ? 1 : 0;
    paint.confidence[index] = hole[index] == 0 ? 1.0F : 0.0F;
  }
  paint.forbidden = dilate(hole, patch, true);
  paint.run();
  Bytes out(static_cast<std::size_t>(W) * H * 4);
  for (int y = 0; y < H; ++y) {
    for (int x = 0; x < W; ++x) {
      const std::size_t from = (static_cast<std::size_t>(y) * width + x) * 4;
      for (int channel = 0; channel < 3; ++channel) {
        const float value = std::round(paint.image[at(x, y) * 3 + channel]);
        out[at(x, y) * 4 + channel] = static_cast<std::uint8_t>(std::clamp(value, 0.0F, 255.0F));
      }
      out[at(x, y) * 4 + 3] = rgba[from + 3];
    }
  }
  return out;
}

void write_scrubbed_town_map(const std::filesystem::path &media,
                             const std::filesystem::path &output) {
  const Bytes pack = read_file(media / town_map_pack);
  for (const PackTexture &texture : parse_texture_pack(pack)) {
    if (texture.index == 0) {
      const Bytes rgba = texture.rgba(pack);
      write_png(output / scrubbed_town_map, W, H,
                scrub_town_map(texture.format.width, texture.format.height, rgba));
      return;
    }
  }
  throw std::runtime_error(std::string(town_map_pack) + " has no texture 0");
}

} // namespace phantomcrash::extract
