#pragma once

#include "desktop_frame.hpp"
#include "desktop_screen.hpp"
#include "desktop_textures.hpp"
#include "phantomcrash/location_menu.hpp"
#include "phantomcrash/glyph_font.hpp"

#include <string>
#include <vector>

namespace phantomcrash {

// A location's menu (LocationMenu: Club Wired, the garage): the background, the turning wheel, the
// help panel and its scrolling line, the lit wedge and the item's name, and
// the town frame (day, money, music meter and credit). The frame style and
// credits outlive the screen.
class LocationMenuScreen final : public Screen {
public:
  LocationMenuScreen(TextureCache &textures, const GlyphFont &font, LocationMenuScript script,
                     const std::string &from, const FrameStyle &frame,
                     const std::vector<MusicCredit> &credits, QuestSave quest);

  void tick(const InputFrame &input, double tick_seconds) override;
  void draw(const Viewport &viewport) override;
  [[nodiscard]] std::string music() const override {
    return menu_.script().music;
  }
  void set_music(const std::string &file) override {
    frame_.set_music(file);
  }
  void hear_music(const std::vector<float> &levels) override {
    frame_.hear_music(levels);
  }
  [[nodiscard]] std::vector<std::string> take_sounds() override {
    return menu_.take_sounds();
  }
  [[nodiscard]] bool done() const override {
    return menu_.done();
  }
  [[nodiscard]] std::string next() const override {
    return menu_.next();
  }

private:
  TextureCache &textures_;
  const GlyphFont &font_;
  LocationMenu menu_;
  LocationFrame frame_;
  QuestSave quest_;
  std::vector<std::vector<int>> help_runs_;
};

} // namespace phantomcrash
