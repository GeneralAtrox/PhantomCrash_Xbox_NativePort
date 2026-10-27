#include "phantomcrash/image_theme.hpp"

#include <algorithm>
#include <cmath>
#include <stdexcept>
#include <vector>

namespace phantomcrash {
namespace {

struct Hsv {
  float h{}; // degrees
  float s{};
  float v{};
};

Hsv to_hsv(const float r, const float g, const float b) {
  const float high = std::max({r, g, b});
  const float low = std::min({r, g, b});
  const float span = high - low;
  Hsv out{0.0F, high > 0.0F ? span / high : 0.0F, high};
  if (span > 0.0F) {
    if (high == r) {
      out.h = 60.0F * std::fmod((g - b) / span + 6.0F, 6.0F);
    } else if (high == g) {
      out.h = 60.0F * ((b - r) / span + 2.0F);
    } else {
      out.h = 60.0F * ((r - g) / span + 4.0F);
    }
  }
  return out;
}

std::array<float, 3> to_rgb(const Hsv &hsv) {
  const float chroma = hsv.v * hsv.s;
  const float sector = std::fmod(hsv.h / 60.0F, 6.0F);
  const float x = chroma * (1.0F - std::abs(std::fmod(sector, 2.0F) - 1.0F));
  const float m = hsv.v - chroma;
  std::array<float, 3> rgb{};
  switch (static_cast<int>(sector)) {
  case 0:
    rgb = {chroma, x, 0.0F};
    break;
  case 1:
    rgb = {x, chroma, 0.0F};
    break;
  case 2:
    rgb = {0.0F, chroma, x};
    break;
  case 3:
    rgb = {0.0F, x, chroma};
    break;
  case 4:
    rgb = {x, 0.0F, chroma};
    break;
  default:
    rgb = {chroma, 0.0F, x};
    break;
  }
  return {rgb[0] + m, rgb[1] + m, rgb[2] + m};
}

bool in_hues(const ImageTheme &theme, const float hue) {
  return std::ranges::any_of(theme.hues, [hue](const auto &range) {
    return range[0] <= range[1] ? hue >= range[0] && hue <= range[1]
                                : hue >= range[0] || hue <= range[1];
  });
}

using Texel = std::array<float, 4>;

Texel texel_at(const std::uint8_t *pixels, const int pitch, const int x, const int y) {
  const std::uint8_t *texel =
      pixels + static_cast<std::ptrdiff_t>(y) * pitch + static_cast<std::ptrdiff_t>(x) * 4;
  return {static_cast<float>(texel[0]), static_cast<float>(texel[1]), static_cast<float>(texel[2]),
          static_cast<float>(texel[3])};
}

// Fills the region from the texels around it (a Coons patch over its four
// borders), so a painted name leaves a smooth gradient of what surrounds it.
void erase_region(const std::array<int, 4> &region, std::uint8_t *pixels, const int width,
                  const int height, const int pitch) {
  const int left = std::clamp(region[0], 0, width);
  const int right = std::clamp(region[2], 0, width);
  const int top = std::clamp(region[1], 0, height);
  const int bottom = std::clamp(region[3], 0, height);
  if (right <= left || bottom <= top) {
    return;
  }
  const int outer_left = std::max(left - 1, 0);
  const int outer_right = std::min(right, width - 1);
  const int outer_top = std::max(top - 1, 0);
  const int outer_bottom = std::min(bottom, height - 1);
  const auto at = [&](const int x, const int y) { return texel_at(pixels, pitch, x, y); };
  std::vector<Texel> above;
  std::vector<Texel> below;
  for (int x = left; x < right; ++x) {
    above.push_back(at(x, outer_top));
    below.push_back(at(x, outer_bottom));
  }
  std::vector<Texel> before;
  std::vector<Texel> after;
  for (int y = top; y < bottom; ++y) {
    before.push_back(at(outer_left, y));
    after.push_back(at(outer_right, y));
  }
  const Texel top_left = at(outer_left, outer_top);
  const Texel top_right = at(outer_right, outer_top);
  const Texel bottom_left = at(outer_left, outer_bottom);
  const Texel bottom_right = at(outer_right, outer_bottom);
  for (int y = top; y < bottom; ++y) {
    const float v = static_cast<float>(y - top + 1) / static_cast<float>(bottom - top + 1);
    const auto row = static_cast<std::size_t>(y - top);
    for (int x = left; x < right; ++x) {
      const float u = static_cast<float>(x - left + 1) / static_cast<float>(right - left + 1);
      const auto column = static_cast<std::size_t>(x - left);
      std::uint8_t *texel =
          pixels + static_cast<std::ptrdiff_t>(y) * pitch + static_cast<std::ptrdiff_t>(x) * 4;
      for (std::size_t channel = 0; channel < 4; ++channel) {
        const float value =
            (1.0F - v) * above[column][channel] + v * below[column][channel] +
            (1.0F - u) * before[row][channel] + u * after[row][channel] -
            ((1.0F - u) * (1.0F - v) * top_left[channel] + u * (1.0F - v) * top_right[channel] +
             (1.0F - u) * v * bottom_left[channel] + u * v * bottom_right[channel]);
        texel[channel] = static_cast<std::uint8_t>(std::lround(std::clamp(value, 0.0F, 255.0F)));
      }
    }
  }
}

void recolour_region(const ImageTheme &theme, const std::array<int, 4> &region,
                     std::uint8_t *pixels, const int width, const int height, const int pitch) {
  const int left = std::clamp(region[0], 0, width);
  const int right = std::clamp(region[2], 0, width);
  for (int y = std::clamp(region[1], 0, height); y < std::clamp(region[3], 0, height); ++y) {
    std::uint8_t *row = pixels + static_cast<std::ptrdiff_t>(y) * pitch;
    for (int x = left; x < right; ++x) {
      std::uint8_t *texel = row + static_cast<std::ptrdiff_t>(x) * 4;
      Hsv hsv = to_hsv(texel[0] / 255.0F, texel[1] / 255.0F, texel[2] / 255.0F);
      if (hsv.s < theme.min_saturation || !in_hues(theme, hsv.h)) {
        continue;
      }
      hsv.h = theme.hue;
      hsv.s = std::clamp(hsv.s * theme.saturation, 0.0F, 1.0F);
      hsv.v = std::clamp(hsv.v * theme.value, 0.0F, 1.0F);
      const auto rgb = to_rgb(hsv);
      for (std::size_t channel = 0; channel < 3; ++channel) {
        texel[channel] = static_cast<std::uint8_t>(std::lround(rgb[channel] * 255.0F));
      }
    }
  }
}

std::array<int, 4> int_rect(const json::JsonValue *value) {
  if (value == nullptr) {
    throw std::runtime_error("themes.json: a region is [left, top, right, bottom]");
  }
  std::array<int, 4> rect{};
  const auto &numbers = value->array();
  for (std::size_t i = 0; i < 4 && i < numbers.size(); ++i) {
    rect[i] = static_cast<int>(numbers[i].number());
  }
  return rect;
}

std::vector<ImageThemeArea> areas(const json::JsonValue &entry, const std::string_view key) {
  std::vector<ImageThemeArea> out;
  if (const auto *listed = entry.find(key)) {
    for (const auto &area : listed->array()) {
      out.push_back({area.string_or("image", ""), int_rect(area.find("region"))});
    }
  }
  return out;
}

std::array<float, 2> hue_range(const json::JsonValue &range) {
  const auto &values = range.array();
  if (values.size() != 2) {
    throw std::runtime_error("themes.json: a hue range is [from, to] degrees");
  }
  return {static_cast<float>(values[0].number()), static_cast<float>(values[1].number())};
}

} // namespace

ImageThemes parse_image_themes(const json::JsonValue &document) {
  ImageThemes themes;
  const json::JsonValue *listed = document.find("themes");
  if (listed == nullptr) {
    return themes;
  }
  for (const auto &[name, entry] : listed->object()) {
    ImageTheme theme;
    if (const auto *hues = entry.find("hues")) {
      for (const auto &range : hues->array()) {
        theme.hues.push_back(hue_range(range));
      }
    }
    theme.min_saturation = static_cast<float>(entry.number_or("min_saturation", 0.0));
    theme.hue = static_cast<float>(entry.number_or("hue", 0.0));
    theme.saturation = static_cast<float>(entry.number_or("saturation", 1.0));
    theme.value = static_cast<float>(entry.number_or("value", 1.0));
    theme.recolour = areas(entry, "recolour");
    theme.erase = areas(entry, "erase");
    if (const auto *pastes = entry.find("paste")) {
      for (const auto &paste : pastes->array()) {
        theme.paste.push_back({paste.string_or("image", ""), paste.string_or("picture", ""),
                               int_rect(paste.find("rect")), int_rect(paste.find("region")),
                               static_cast<float>(paste.number_or("max_scale", 1.0)),
                               paste.boolean_or("clear", false)});
      }
    }
    themes.emplace(name, std::move(theme));
  }
  return themes;
}

ThemedImage split_themed_image(const std::string_view key) {
  const auto mark = key.rfind('#');
  if (mark == std::string_view::npos) {
    return {key, {}};
  }
  return {key.substr(0, mark), key.substr(mark + 1)};
}

std::uint32_t themed_colour(const ImageTheme &theme, const std::uint32_t rgb) {
  std::array<std::uint8_t, 4> texel{static_cast<std::uint8_t>(rgb >> 16U),
                                    static_cast<std::uint8_t>(rgb >> 8U),
                                    static_cast<std::uint8_t>(rgb), 0xff};
  recolour_region(theme, {0, 0, 1, 1}, texel.data(), 1, 1, 4);
  return (std::uint32_t{texel[0]} << 16U) | (std::uint32_t{texel[1]} << 8U) | texel[2];
}

void apply_image_theme(const ImageTheme &theme, const std::string_view image, std::uint8_t *pixels,
                       const int width, const int height, const int pitch) {
  for (const auto &area : theme.erase) {
    if (area.image == image) {
      erase_region(area.region, pixels, width, height, pitch);
    }
  }
  if (theme.hues.empty()) {
    return;
  }
  if (theme.recolour.empty()) {
    recolour_region(theme, {0, 0, width, height}, pixels, width, height, pitch);
  }
  for (const auto &area : theme.recolour) {
    if (area.image == image) {
      recolour_region(theme, area.region, pixels, width, height, pitch);
    }
  }
}

} // namespace phantomcrash
