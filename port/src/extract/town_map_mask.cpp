// Masks over the town map picture: growing, shrinking and filling them.
#include "town_map_mask.hpp"

#include <algorithm>
#include <array>
#include <vector>

namespace phantomcrash::extract::town_mask {
namespace {

constexpr int W = town_map_width;
constexpr int H = town_map_height;

} // namespace

std::size_t at(const int x, const int y) {
  return static_cast<std::size_t>(y) * W + static_cast<std::size_t>(x);
}

bool inside(const int x, const int y) {
  return x >= 0 && y >= 0 && x < W && y < H;
}

// Grown by one texel `iterations` times, to the four neighbours or, when
// `square`, all eight.
Mask dilate(Mask mask, const int iterations, const bool square) {
  for (int step = 0; step < iterations; ++step) {
    Mask grown = mask;
    for (int y = 0; y < H; ++y) {
      for (int x = 0; x < W; ++x) {
        if (mask[at(x, y)] != 0) {
          continue;
        }
        for (int dy = -1; dy <= 1 && grown[at(x, y)] == 0; ++dy) {
          for (int dx = -1; dx <= 1; ++dx) {
            if ((square || dx == 0 || dy == 0) && inside(x + dx, y + dy) &&
                mask[at(x + dx, y + dy)] != 0) {
              grown[at(x, y)] = 1;
              break;
            }
          }
        }
      }
    }
    mask = std::move(grown);
  }
  return mask;
}

// Shrunk by one texel to the four neighbours `iterations` times; beyond
// the picture counts as clear.
Mask erode(Mask mask, const int iterations) {
  constexpr std::array<std::array<int, 2>, 4> around{{{1, 0}, {-1, 0}, {0, 1}, {0, -1}}};
  for (int step = 0; step < iterations; ++step) {
    Mask shrunk = mask;
    for (int y = 0; y < H; ++y) {
      for (int x = 0; x < W; ++x) {
        for (const auto &[dx, dy] : around) {
          if (!inside(x + dx, y + dy) || mask[at(x + dx, y + dy)] == 0) {
            shrunk[at(x, y)] = 0;
          }
        }
      }
    }
    mask = std::move(shrunk);
  }
  return mask;
}

// With every clear area that cannot reach the picture's edge filled.
Mask fill_holes(const Mask &mask) {
  Mask outside(mask.size(), 0);
  std::vector<std::array<int, 2>> open;
  const auto reach = [&](const int x, const int y) {
    if (inside(x, y) && mask[at(x, y)] == 0 && outside[at(x, y)] == 0) {
      outside[at(x, y)] = 1;
      open.push_back({x, y});
    }
  };
  for (int x = 0; x < W; ++x) {
    reach(x, 0);
    reach(x, H - 1);
  }
  for (int y = 0; y < H; ++y) {
    reach(0, y);
    reach(W - 1, y);
  }
  while (!open.empty()) {
    const auto [x, y] = open.back();
    open.pop_back();
    reach(x + 1, y);
    reach(x - 1, y);
    reach(x, y + 1);
    reach(x, y - 1);
  }
  Mask out(mask.size());
  for (std::size_t index = 0; index < mask.size(); ++index) {
    out[index] = outside[index] == 0 ? 1 : 0;
  }
  return out;
}

Mask shifted(const Mask &mask, const std::array<int, 2> &by) {
  Mask out(mask.size(), 0);
  for (int y = 0; y < H; ++y) {
    for (int x = 0; x < W; ++x) {
      if (inside(x - by[0], y - by[1]) && mask[at(x - by[0], y - by[1])] != 0) {
        out[at(x, y)] = 1;
      }
    }
  }
  return out;
}

Mask either(Mask a, const Mask &b) {
  for (std::size_t index = 0; index < a.size(); ++index) {
    a[index] = static_cast<std::uint8_t>(a[index] | b[index]);
  }
  return a;
}

// `mask` kept only inside the box (left, top, right, bottom; right and
// bottom exclusive).
Mask within(Mask mask, const std::array<int, 4> &box) {
  for (int y = 0; y < H; ++y) {
    for (int x = 0; x < W; ++x) {
      if (x < box[0] || y < box[1] || x >= box[2] || y >= box[3]) {
        mask[at(x, y)] = 0;
      }
    }
  }
  return mask;
}

} // namespace phantomcrash::extract::town_mask
