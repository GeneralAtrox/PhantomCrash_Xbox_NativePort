#pragma once

#include "desktop_town_screen.hpp"
#include "phantomcrash/slai_rumble_hub.hpp"

#include <optional>
#include <string>
#include <vector>

namespace phantomcrash {

class DrawList;

// Rumble S.L.A.I. (SlaiRumbleHub): today's cities, each with its plate,
// venue tile and badge, the regulation, the money entry needs and the
// prize; the cursor or EXIT; a rumble's INFORMATION; the town frame. The
// S.L.A.I. data outlives the screen.
class SlaiRumbleScreen final : public TownScreen {
public:
  SlaiRumbleScreen(TextureCache &textures, const GlyphFont &font, SlaiRumbleHubScript script,
                   const SlaiRumbleData &data, const std::string &from, const std::string &back,
                   const FrameStyle &frame, const std::vector<MusicCredit> &credits,
                   const QuestSave &quest, const MessageBoxStyle &box);

  void tick(const InputFrame &input, double tick_seconds) override;
  void draw(const Viewport &viewport) override;
  [[nodiscard]] std::vector<std::string> take_sounds() override {
    return hub_.take_sounds();
  }
  [[nodiscard]] bool done() const override {
    return hub_.done();
  }
  [[nodiscard]] std::string next() const override {
    return hub_.next();
  }
  // The rumble entered, once the screen leaves for its battle.
  [[nodiscard]] std::optional<SlaiCityMatch> entered() const {
    return hub_.entered();
  }

private:
  void draw_row(DrawList &list, int row) const;
  void draw_cursor(DrawList &list) const;
  void draw_popup(DrawList &list) const;

  const NumberFont &money_font_;
  SlaiRumbleHub hub_;
};

} // namespace phantomcrash
