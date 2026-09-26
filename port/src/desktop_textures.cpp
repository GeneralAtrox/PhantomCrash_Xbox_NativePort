#include "desktop_textures.hpp"

#include <SDL3_image/SDL_image.h>

namespace phantomcrash {

TextureCache::TextureCache(SDL_Renderer *renderer, const DataRoots &roots)
    : renderer_(renderer), roots_(roots) {}

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
    loaded = IMG_LoadTexture(renderer_, path->string().c_str());
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
  SDL_FRect source{left, top, right - left, bottom - top};
  // A sprite stretched from a few texels (the title's 8-texel bands) would
  // blend in its atlas neighbours at the edges; sample from texel centres.
  const auto quads = place_sprite(sprite, seconds);
  if (!quads.empty() && quads.front().width > source.w * 2.0F) {
    source.x += 0.5F;
    source.w -= 1.0F;
  }
  if (!quads.empty() && quads.front().height > source.h * 2.0F) {
    source.y += 0.5F;
    source.h -= 1.0F;
  }
  for (const auto &quad : quads) {
    const auto pixels = viewport.to_pixels(quad.x, quad.y, quad.width, quad.height);
    const SDL_FRect target{pixels.x, pixels.y, pixels.width, pixels.height};
    SDL_RenderTexture(textures.renderer(), texture, &source, &target);
  }
}

} // namespace phantomcrash
