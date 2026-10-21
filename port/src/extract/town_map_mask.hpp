// Masks over the town map picture (town_map_width x town_map_height, one
// byte a texel, non-zero set), for its scrub.
#pragma once

#include "town_map_scrub.hpp"

#include <array>
#include <cstddef>
#include <cstdint>
#include <vector>

namespace phantomcrash::extract::town_mask {

using Mask = std::vector<std::uint8_t>;

[[nodiscard]] std::size_t at(int x, int y);
[[nodiscard]] bool inside(int x, int y);
// Grown by one texel `iterations` times, to the four neighbours or, when
// `square`, all eight.
[[nodiscard]] Mask dilate(Mask mask, int iterations, bool square = false);
// Shrunk by one texel to the four neighbours `iterations` times; beyond
// the picture counts as clear.
[[nodiscard]] Mask erode(Mask mask, int iterations);
// With every clear area that cannot reach the picture's edge filled.
[[nodiscard]] Mask fill_holes(const Mask &mask);
[[nodiscard]] Mask shifted(const Mask &mask, const std::array<int, 2> &by);
[[nodiscard]] Mask either(Mask a, const Mask &b);
// `mask` kept only inside the box (left, top, right, bottom; right and
// bottom exclusive).
[[nodiscard]] Mask within(Mask mask, const std::array<int, 4> &box);

} // namespace phantomcrash::extract::town_mask
