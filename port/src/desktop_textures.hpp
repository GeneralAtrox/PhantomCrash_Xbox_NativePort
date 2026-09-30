#pragma once

#include "phantomcrash/canvas.hpp"
#include "phantomcrash/data_roots.hpp"
#include "phantomcrash/json.hpp"
#include "phantomcrash/sprite_layer.hpp"
#include "desktop_replay_overlay.hpp"

#include <SDL3/SDL.h>

#include <map>
#include <string>

namespace phantomcrash {

// Images named by the data files, loaded through the data roots on first use
// and kept for the whole run (the front end uses two textures).
class TextureCache final {
public:
  TextureCache(SDL_Renderer *renderer, const DataRoots &roots);
  ~TextureCache();
  TextureCache(const TextureCache &) = delete;
  TextureCache &operator=(const TextureCache &) = delete;

  // Null when the image is missing (logged once).
  [[nodiscard]] SDL_Texture *get(const std::string &file);
  // Loads every image a data document names (each string ending ".png"),
  // so a screen's first use of one does not load it mid-frame.
  void preload(const json::JsonValue &document);
  void record_into(ReplayOverlay *overlay) { recording_ = overlay; }
  ReplayOverlay *recording() const { return recording_; }

  [[nodiscard]] SDL_Renderer *renderer() const noexcept {
    return renderer_;
  }

private:
  SDL_Renderer *renderer_{};
  ReplayOverlay *recording_{};
  const DataRoots &roots_;
  std::map<std::string, SDL_Texture *> textures_;
};

// Draws a sprite's quads with its animation and colour, times `alpha`.
void draw_sprite(TextureCache &textures, const Sprite &sprite, const Viewport &viewport,
                 double seconds, float alpha = 1.0F, std::uint32_t colour = 0xffffff);

} // namespace phantomcrash
