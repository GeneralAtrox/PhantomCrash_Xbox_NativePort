// Atlas art with its painted lettering erased, for display text drawn over
// it (desktop_sprite_text.cpp).

#include "desktop_textures.hpp"

#include <SDL3_image/SDL_image.h>

#include <algorithm>
#include <array>
#include <cmath>
#include <string>

namespace phantomcrash {

namespace {

// Each row of `region` blended from the texel left of it to the texel right
// of it: the flat and vertically shaded fills behind the menus' lettering.
void erase_rows(SDL_Surface *surface, const EraseArea &area) {
  const auto &region = area.region;
  const auto fill = area.fill;
  const int width = surface->w;
  const int height = surface->h;
  const int top = std::clamp(static_cast<int>(std::floor(region[1])), 0, height);
  const int bottom = std::clamp(static_cast<int>(std::ceil(region[3])), top, height);
  auto *pixels = static_cast<std::uint8_t *>(surface->pixels);
  for (int y = top; y < bottom; ++y) {
    const float lean = area.slant * (region[3] - (static_cast<float>(y) + 0.5F));
    const int left = std::clamp(static_cast<int>(std::floor(region[0] + lean)), 0, width);
    const int right = std::clamp(static_cast<int>(std::ceil(region[2] + lean)), left, width);
    std::uint8_t *row = pixels + static_cast<std::ptrdiff_t>(y) * surface->pitch;
    const int from = std::max(left - 1, 0);
    const int to = std::min(right, width - 1);
    const float span = static_cast<float>(std::max(to - from, 1));
    for (int x = left; x < right; ++x) {
      if (fill) {
        const std::array<std::uint8_t, 4> flat{static_cast<std::uint8_t>(*fill >> 16U),
                                               static_cast<std::uint8_t>(*fill >> 8U),
                                               static_cast<std::uint8_t>(*fill), 255};
        std::copy(flat.begin(), flat.end(), row + static_cast<std::ptrdiff_t>(x) * 4);
        continue;
      }
      const float t = static_cast<float>(x - from) / span;
      for (int channel = 0; channel < 4; ++channel) {
        const float a = row[from * 4 + channel];
        const float b = row[to * 4 + channel];
        row[x * 4 + channel] = static_cast<std::uint8_t>(a + (b - a) * t + 0.5F);
      }
    }
  }
}

} // namespace

SDL_Texture *TextureCache::erased(const std::string &file, const std::array<float, 4> &rect,
                                  const std::vector<EraseArea> &areas) {
  std::string key = file;
  for (const float value : rect) {
    key += "," + std::to_string(value);
  }
  for (const auto &area : areas) {
    key += area.fill ? "#" + std::to_string(*area.fill) : std::string{";"};
    key += "/" + std::to_string(area.slant);
    for (const float value : area.region) {
      key += "," + std::to_string(value);
    }
  }
  if (const auto found = erased_.find(key); found != erased_.end()) {
    return found->second;
  }
  SDL_Texture *texture = nullptr;
  if (const auto path = roots_.find_asset(file)) {
    if (SDL_Surface *loaded = IMG_Load(path->string().c_str())) {
      SDL_Surface *atlas = SDL_ConvertSurface(loaded, SDL_PIXELFORMAT_RGBA32);
      SDL_DestroySurface(loaded);
      const auto x = static_cast<int>(rect[0]), y = static_cast<int>(rect[1]);
      const auto w = static_cast<int>(rect[2] - rect[0]), h = static_cast<int>(rect[3] - rect[1]);
      SDL_Surface *piece = atlas != nullptr && w > 0 && h > 0
                               ? SDL_CreateSurface(w, h, SDL_PIXELFORMAT_RGBA32)
                               : nullptr;
      if (piece != nullptr) {
        const SDL_Rect from{x, y, w, h};
        SDL_SetSurfaceBlendMode(atlas, SDL_BLENDMODE_NONE);
        SDL_BlitSurface(atlas, &from, piece, nullptr);
        for (const auto &area : areas) {
          erase_rows(piece, area);
        }
        texture = SDL_CreateTextureFromSurface(renderer_, piece);
        SDL_DestroySurface(piece);
      }
      SDL_DestroySurface(atlas);
    }
  }
  if (texture != nullptr) {
    SDL_SetTextureScaleMode(texture, SDL_SCALEMODE_LINEAR);
    SDL_SetTextureBlendMode(texture, SDL_BLENDMODE_BLEND);
  }
  erased_[key] = texture;
  return texture;
}

} // namespace phantomcrash
