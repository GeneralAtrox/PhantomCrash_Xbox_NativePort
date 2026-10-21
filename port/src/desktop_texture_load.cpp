#include "desktop_textures.hpp"

#include <SDL3_image/SDL_image.h>

#include <algorithm>
#include <cmath>

namespace phantomcrash {
namespace {

SDL_Rect sdl_rect(const std::array<int, 4> &ltrb) {
  return {ltrb[0], ltrb[1], ltrb[2] - ltrb[0], ltrb[3] - ltrb[1]};
}

} // namespace

const ImageThemes &TextureCache::image_themes() {
  if (!image_themes_) {
    image_themes_.emplace();
    if (roots_.has_document("ui/themes.json")) {
      try {
        *image_themes_ = parse_image_themes(roots_.document("ui/themes.json"));
      } catch (const std::exception &error) {
        SDL_Log("ui/themes.json: %s", error.what());
      }
    }
  }
  return *image_themes_;
}

void TextureCache::paste_pictures(const ImageTheme &theme, const std::string_view image,
                                  SDL_Surface *target) {
  for (const auto &paste : theme.paste) {
    if (paste.image != image) {
      continue;
    }
    const SDL_Rect region = sdl_rect(paste.region);
    if (paste.clear) {
      SDL_FillSurfaceRect(target, &region, 0);
    }
    SDL_Surface *picture = load_rgba(paste.picture);
    if (picture == nullptr) {
      SDL_Log("Theme picture %s is missing", paste.picture.c_str());
      continue;
    }
    const SDL_Rect source = sdl_rect(paste.rect);
    const float scale =
        std::min({static_cast<float>(region.w) / static_cast<float>(source.w),
                  static_cast<float>(region.h) / static_cast<float>(source.h), paste.max_scale});
    const int width = static_cast<int>(std::lround(static_cast<float>(source.w) * scale));
    const int height = static_cast<int>(std::lround(static_cast<float>(source.h) * scale));
    SDL_Rect placed{region.x + (region.w - width) / 2, region.y + (region.h - height) / 2, width,
                    height};
    SDL_SetSurfaceBlendMode(picture, SDL_BLENDMODE_BLEND);
    SDL_BlitSurfaceScaled(picture, &source, target, &placed, SDL_SCALEMODE_LINEAR);
    SDL_DestroySurface(picture);
  }
}

SDL_Surface *TextureCache::load_rgba(const std::string &key) {
  const auto [image, theme_name] = split_themed_image(key);
  const auto path = roots_.find_asset(image);
  SDL_Surface *loaded = path ? IMG_Load(path->string().c_str()) : nullptr;
  if (loaded == nullptr) {
    return nullptr;
  }
  SDL_Surface *rgba = SDL_ConvertSurface(loaded, SDL_PIXELFORMAT_RGBA32);
  SDL_DestroySurface(loaded);
  if (rgba == nullptr || theme_name.empty()) {
    return rgba;
  }
  const auto &themes = image_themes();
  const auto theme = themes.find(theme_name);
  if (theme == themes.end()) {
    SDL_Log("Image %s names an unknown theme", key.c_str());
    return rgba;
  }
  if (SDL_LockSurface(rgba)) {
    apply_image_theme(theme->second, image, static_cast<std::uint8_t *>(rgba->pixels), rgba->w,
                      rgba->h, rgba->pitch);
    SDL_UnlockSurface(rgba);
  }
  paste_pictures(theme->second, image, rgba);
  return rgba;
}

} // namespace phantomcrash
