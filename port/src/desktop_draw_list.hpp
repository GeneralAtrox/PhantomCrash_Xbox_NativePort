#pragma once

#include "desktop_textures.hpp"
#include "phantomcrash/canvas.hpp"
#include "phantomcrash/glyph_font.hpp"
#include "phantomcrash/message_box.hpp"
#include "phantomcrash/sprite_layer.hpp"

#include <array>
#include <cstdint>
#include <functional>
#include <optional>
#include <vector>

namespace phantomcrash {

// Sprites and text collected for one frame and drawn lowest layer first, in
// the order added within a layer, as the original's layered 2D lists draw.
class DrawList final {
public:
  DrawList(TextureCache &textures, const GlyphFont &font, const Viewport &viewport, double seconds)
      : textures_(textures), font_(font), viewport_(viewport), seconds_(seconds) {}

  void sprite(const Sprite &source);
  void text(int layer, std::vector<GlyphQuad> glyphs, std::uint32_t colour,
            const std::optional<std::array<float, 4>> &clip = std::nullopt);
  // A ticker `seconds` after its text was set.
  void ticker(const TickerStyle &style, const std::vector<int> &run, double seconds);
  // A message box as it is now: the growing window, then its lines and choices.
  void message_box(const MessageBox &box);
  // Draws everything collected, then forgets it.
  void flush();

private:
  struct Command {
    int layer{};
    std::function<void()> draw;
  };

  TextureCache &textures_;
  const GlyphFont &font_;
  const Viewport &viewport_;
  double seconds_;
  std::vector<Command> commands_;
};

// Clears the frame to black.
void clear_black(TextureCache &textures);
// Covers the whole window with black at `alpha` (the screens' fades).
void fill_black(TextureCache &textures, float alpha);

} // namespace phantomcrash
