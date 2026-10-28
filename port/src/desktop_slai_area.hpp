#pragma once

#include "desktop_rumbling_look.hpp"
#include "desktop_sv_purchase.hpp"
#include "desktop_town_screen.hpp"
#include "phantomcrash/slai_area_select.hpp"

#include <optional>
#include <string>
#include <vector>

namespace phantomcrash {

class DrawList;

// S.L.A.I.'s area select (SlaiAreaSelect), drawn as RUMBLING is: the
// plates, the city's blueprint in the picture frame with its emblem, label
// and venue as the title, its rumble on the info panel, then RUMBLING's
// sub-menu over the stage's map, its gates and INFORMATION. The S.L.A.I.
// data outlives the screen; `remembered_area` keeps the area the sub-menu
// was on for coming back from SELECT SV.
class SlaiAreaScreen final : public TownScreen {
public:
  SlaiAreaScreen(TextureCache &textures, const GlyphFont &font, SlaiAreaSelect logic,
                 const MessageBoxStyle &box, const std::string &from, const FrameStyle &frame,
                 const std::vector<MusicCredit> &credits, const QuestSave &quest,
                 int &remembered_area);

  void tick(const InputFrame &input, double tick_seconds) override;
  void draw(const Viewport &viewport) override;
  [[nodiscard]] std::vector<std::string> take_sounds() override;
  [[nodiscard]] bool done() const override {
    return logic_.done();
  }
  [[nodiscard]] std::string next() const override {
    return logic_.next();
  }
  [[nodiscard]] const QuestSave *quest_save() const override {
    return logic_.done() ? &logic_.quest() : nullptr;
  }
  [[nodiscard]] const QuestSave *pending_quest() const override {
    return &logic_.quest();
  }
  // The rumble entered, once the screen leaves for its battle.
  [[nodiscard]] std::optional<SlaiAreaEntry> entered() const {
    return logic_.done() ? logic_.entry() : std::nullopt;
  }

private:
  [[nodiscard]] RumblingLook look() const;
  void draw_area(DrawList &list, int area, float alpha) const;
  void draw_title(DrawList &list, int area, const std::array<float, 2> &at, float alpha) const;
  void draw_info(DrawList &list) const;
  void draw_details(DrawList &list) const;
  [[nodiscard]] const SlaiRegulation *rule(int area) const;

  SlaiAreaSelect logic_;
  const HudStyle &hud_;
  ShopNotice notice_;
  int &remembered_area_;
};

} // namespace phantomcrash
