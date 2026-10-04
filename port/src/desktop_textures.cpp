#include "desktop_textures.hpp"
#include "desktop_sharp_text.hpp"

#include <SDL3_image/SDL_image.h>

#include <vector>

namespace phantomcrash {

namespace {

// Transparent texels keep whatever colour the art left in them (grey and
// red in the talk characters). Filtering a scaled-up sprite blends that
// colour into its edges, a light outline round every face part that the
// original, drawing texel for texel at 640x480, never shows. Each
// transparent texel takes its visible neighbours' average colour instead,
// staying transparent.
void bleed_edges(SDL_Surface *surface) {
  if (!SDL_LockSurface(surface)) {
    return;
  }
  auto *pixels = static_cast<Uint8 *>(surface->pixels);
  const int pitch = surface->pitch;
  const std::vector<Uint8> source(pixels, pixels + static_cast<std::size_t>(pitch) * surface->h);
  const auto at = [&](const int x, const int y) {
    return static_cast<std::size_t>(y) * static_cast<std::size_t>(pitch) +
           static_cast<std::size_t>(x) * 4U;
  };
  for (int y = 0; y < surface->h; ++y) {
    for (int x = 0; x < surface->w; ++x) {
      if (source[at(x, y) + 3] != 0) {
        continue;
      }
      int sum[3]{};
      int count = 0;
      for (int dy = -1; dy <= 1; ++dy) {
        for (int dx = -1; dx <= 1; ++dx) {
          const int nx = x + dx;
          const int ny = y + dy;
          if (nx < 0 || ny < 0 || nx >= surface->w || ny >= surface->h ||
              source[at(nx, ny) + 3] == 0) {
            continue;
          }
          for (int channel = 0; channel < 3; ++channel) {
            sum[channel] += source[at(nx, ny) + static_cast<std::size_t>(channel)];
          }
          ++count;
        }
      }
      if (count > 0) {
        for (int channel = 0; channel < 3; ++channel) {
          pixels[at(x, y) + static_cast<std::size_t>(channel)] =
              static_cast<Uint8>(sum[channel] / count);
        }
      }
    }
  }
  SDL_UnlockSurface(surface);
}

SDL_Texture *load_texture(SDL_Renderer *renderer, const char *path) {
  SDL_Surface *loaded = IMG_Load(path);
  if (loaded == nullptr) {
    return nullptr;
  }
  SDL_Surface *rgba = SDL_ConvertSurface(loaded, SDL_PIXELFORMAT_RGBA32);
  SDL_DestroySurface(loaded);
  if (rgba == nullptr) {
    return nullptr;
  }
  bleed_edges(rgba);
  SDL_Texture *texture = SDL_CreateTextureFromSurface(renderer, rgba);
  SDL_DestroySurface(rgba);
  return texture;
}

} // namespace

TextureCache::TextureCache(SDL_Renderer *renderer, const DataRoots &roots)
    : renderer_(renderer), roots_(roots) {}

SharpText *TextureCache::sharp_text() {
  if (!sharp_enabled_) {
    return nullptr;
  }
  if (!sharp_) {
    SharpTextStyle style;
    try {
      style = parse_sharp_text(roots_.document("ui/text.json"));
    } catch (const std::exception &error) {
      SDL_Log("ui/text.json: %s", error.what());
    }
    sharp_ = std::make_unique<SharpText>(renderer_, std::move(style));
  }
  return sharp_->available() ? sharp_.get() : nullptr;
}

TextureCache::~TextureCache() {
  for (auto &[file, texture] : textures_) {
    SDL_DestroyTexture(texture);
  }
}

SDL_Texture *TextureCache::get(const std::string &file) {
  if (auto found = textures_.find(file); found != textures_.end()) {
    return found->second;
  }
  SDL_Texture *loaded = nullptr;
  if (const auto path = roots_.find_asset(file)) {
    loaded = load_texture(renderer_, path->string().c_str());
    if (loaded != nullptr) {
      SDL_SetTextureScaleMode(loaded, SDL_SCALEMODE_LINEAR);
      SDL_SetTextureBlendMode(loaded, SDL_BLENDMODE_BLEND);
    }
  }
  if (loaded == nullptr) {
    SDL_Log("Image %s could not be loaded", file.c_str());
  }
  // Cached even when missing, so the log appears once.
  textures_[file] = loaded;
  return loaded;
}

void TextureCache::preload(const json::JsonValue &document) {
  if (document.is_string()) {
    if (document.string().ends_with(".png")) {
      static_cast<void>(get(document.string()));
    }
  } else if (document.is_array()) {
    for (const json::JsonValue &item : document.array()) {
      preload(item);
    }
  } else if (document.is_object()) {
    for (const auto &[key, item] : document.object()) {
      preload(item);
    }
  }
}

void draw_sprite(TextureCache &textures, const Sprite &sprite, const Viewport &viewport,
                 const double seconds, const float alpha, const std::uint32_t colour) {
  SDL_Texture *texture = textures.get(sprite.image);
  const float opacity = alpha * animation_alpha(sprite.animation, seconds);
  if (texture == nullptr || opacity <= 0.0F) {
    return;
  }
  // Both colours multiply: the sprite's own and the caller's (greyed items).
  const auto channel = [&](const int shift) {
    const auto own = static_cast<float>((sprite.colour >> shift) & 0xff);
    const auto tint = static_cast<float>((colour >> shift) & 0xff);
    return static_cast<Uint8>(own * tint / 255.0F + 0.5F);
  };
  SDL_SetTextureColorMod(texture, channel(16), channel(8), channel(0));
  SDL_SetTextureAlphaMod(texture, static_cast<Uint8>(opacity * 255.0F + 0.5F));
  const auto [left, top, right, bottom] = sprite.rect;
  const SDL_FRect rect{left, top, right - left, bottom - top};
  // Scaled up, filtering at a quad's edge reaches past its rectangle into
  // the atlas neighbours (a line along a face part, a seam between window
  // pieces). Inset the rectangle so the edge pixels sample inside it:
  // nothing texel for texel, as the original draws, up to half a texel
  // when stretched far (the title's 8-texel bands).
  const auto inset = [](const float pixels, const float texels) {
    const float scale = texels > 1.0F ? pixels / texels : 1.0F;
    return scale > 1.0F ? 0.5F - 0.5F / scale : 0.0F;
  };
  for (const auto &quad : place_sprite(sprite, seconds)) {
    const auto pixels = viewport.to_pixels(quad.x, quad.y, quad.width, quad.height);
    SDL_FRect target{pixels.x, pixels.y, pixels.width, pixels.height};
    const float inset_x = inset(pixels.width, rect.w);
    const float inset_y = inset(pixels.height, rect.h);
    SDL_FRect source{rect.x + inset_x, rect.y + inset_y, rect.w - 2.0F * inset_x,
                     rect.h - 2.0F * inset_y};
    if (sprite.angle == 0.0F && !sprite.flip_x) {
      draw_sprite_edges(textures, texture, sprite, viewport, source, target);
      record_quad(textures, texture, source, target);
      SDL_RenderTexture(textures.renderer(), texture, &source, &target);
      continue;
    }
    const SDL_FPoint pivot = sprite.pivot ? SDL_FPoint{(*sprite.pivot)[0] * viewport.scale,
                                                       (*sprite.pivot)[1] * viewport.scale}
                                          : SDL_FPoint{target.w * 0.5F, target.h * 0.5F};
    record_quad(textures, texture, source, target, sprite.angle, pivot,
                sprite.flip_x ? SDL_FLIP_HORIZONTAL : SDL_FLIP_NONE);
    SDL_RenderTextureRotated(textures.renderer(), texture, &source, &target, sprite.angle, &pivot,
                             sprite.flip_x ? SDL_FLIP_HORIZONTAL : SDL_FLIP_NONE);
  }
}

} // namespace phantomcrash
