#include "desktop_rumbling.hpp"
#include "phantomcrash/money.hpp"

#include "desktop_contest_info.hpp"
#include "desktop_rumbling_look.hpp"
#include "desktop_draw_list.hpp"
#include "desktop_shop_list.hpp"

#include <algorithm>
#include <cmath>
#include <utility>

namespace phantomcrash {
namespace {

Sprite shifted(Sprite sprite, const float dx, const float dy = 0.0F) {
  sprite.position = {sprite.position[0] + dx, sprite.position[1] + dy};
  return sprite;
}

Sprite placed(Sprite sprite, const CanvasPoint &at) {
  sprite.position = at;
  return sprite;
}

float lerp(const float from, const float to, const float t) {
  return from + (to - from) * t;
}

} // namespace

RumblingScreen::RumblingScreen(TextureCache &textures, const GlyphFont &font, Rumbling logic,
                               const ContestTables &contests, const MessageBoxStyle &box,
                               const std::string &from, const FrameStyle &frame,
                               const std::vector<MusicCredit> &credits, QuestSave quest,
                               int &remembered_area)
    : TownScreen(textures, font, logic.script(), from, frame, credits, std::move(quest)),
      logic_(std::move(logic)), contests_(contests), hud_(frame.hud),
      notice_(box, logic_.script().message_text_at), remembered_area_(remembered_area) {}

void RumblingScreen::tick(const InputFrame &input, const double tick_seconds) {
  logic_.update(tick_seconds, input);
  tick_frame(tick_seconds, logic_.faded_in());
  notice_.update(logic_.message(), false, 0, logic_.script().messages, tick_seconds);
  if (logic_.done()) {
    remembered_area_ = logic_.area();
  }
}

std::vector<std::string> RumblingScreen::take_sounds() {
  std::vector<std::string> sounds = logic_.take_sounds();
  for (std::string &name : notice_.take_sounds()) {
    sounds.push_back(std::move(name));
  }
  return sounds;
}

RumblingLook RumblingScreen::look() const {
  RumblingLook look;
  look.area = logic_.area();
  look.view = logic_.view();
  look.items_slide = logic_.items_slide();
  look.bar_slide = logic_.bar_slide();
  look.turn = logic_.turn();
  look.in_menu = logic_.in_menu();
  look.menu_item = logic_.menu_item();
  look.menu_seconds = logic_.menu_seconds();
  look.gate = logic_.gate();
  look.seconds = logic_.seconds();
  look.can_choose = [this](const RumblingAction action) { return logic_.can_choose(action); };
  return look;
}

void RumblingScreen::draw_items(DrawList &list) const {
  draw_rumbling_items(list, logic_.script(), look());
}

// 0x015429: the venue's picture (by day or night) in its frame, the area's
// name strip and its logo; a venue with no contest shows CLOSED over noise.
// Turning to the sub-menu fades them and moves the logo to the menu's top.
void RumblingScreen::draw_area(DrawList &list, const int area, const float alpha) const {
  const RumblingScript &script = logic_.script();
  const RumblingArea &shown = script.areas[static_cast<std::size_t>(area)];
  const float t = logic_.turn();
  const float stay = alpha * (1.0F - t);
  const Contest *held = contests_.contest(logic_.contest(area));
  list.sprite(held != nullptr && held->night ? shown.night_picture : shown.picture, stay);
  list.sprite(shown.label, stay);
  const CanvasPoint &to = script.menu.title_at;
  list.sprite(placed(shown.title, {lerp(shown.title.position[0], to[0], t),
                                   lerp(shown.title.position[1], to[1], t)}),
              alpha);
  if (!shown.exit && held == nullptr) {
    list.sprite(script.area_select.closed, stay);
    list.sprite(script.area_select.noise, stay * script.area_select.noise_alpha);
  }
}

// 0x015c44: the contest's and its class's names, its fee and CLEAR once
// won, sliding off to the right as the sub-menu opens.
void RumblingScreen::draw_info(DrawList &list) const {
  const RumblingAreaSelect &style = logic_.script().area_select;
  const float dx = logic_.script().menu.info_slide * logic_.turn();
  list.sprite(shifted(style.info_panel, dx));
  const int contest = logic_.contest(logic_.area());
  const Contest *held = contests_.contest(contest);
  if (held == nullptr) {
    return;
  }
  const auto right_text = [&](const std::string &text, const float y) {
    const std::vector<int> run = glyph_run(font_, text);
    list.text(style.text_layer,
              layout_run(font_, run, style.info_right + dx - run_width(font_, run), y),
              style.text_colour);
  };
  right_text(held->name, style.contest_y);
  if (const ContestClass *rules = contests_.contest_class(held->contest_class)) {
    right_text(rules->name, style.class_y);
    NumberStyle fee = hud_.money;
    fee.right = {style.fee_at[0] + dx, style.fee_at[1]};
    fee.digits = style.fee_digits;
    fee.colour = style.text_colour;
    for (const Sprite &digit : money_sprites(hud_, fee, shown_ny(rules->fee), style.text_layer)) {
      list.sprite(digit);
    }
  }
  const bool marked = std::find(style.no_clear_mark.begin(), style.no_clear_mark.end(), contest) ==
                      style.no_clear_mark.end();
  if (marked && logic_.won(contest)) {
    list.sprite(style.clear, 1.0F - logic_.turn());
  }
}

void RumblingScreen::draw_menu(DrawList &list) const {
  const RumblingLook shown = look();
  const RumblingScript &script = logic_.script();
  draw_rumbling_menu_items(list, script.menu, shown);
  const RumblingArea &area = script.areas[static_cast<std::size_t>(logic_.area())];
  if (draw_rumbling_map(list, script.menu, area, shown)) {
    draw_robots(list);
  }
}

void RumblingScreen::draw_gates(DrawList &list) const {
  const RumblingScript &script = logic_.script();
  draw_rumbling_gates(list, script.gates, script.areas[static_cast<std::size_t>(logic_.area())],
                      look());
}

// 0x013f5a: the WIRE HEADS DATA window pops open, then the opponent's
// pilot, robot, partner (only with a pilot), team and bounty, the faces
// and the way back.
void RumblingScreen::draw_character_info(DrawList &list) const {
  const RumblingCharacterInfo &info = logic_.script().character_info;
  if (!draw_pop_window(list, info.window, logic_.info_seconds())) {
    return;
  }
  for (const Sprite &hint : info.close_hint) {
    list.sprite(hint);
  }
  const auto &cards = logic_.cards();
  const auto index = static_cast<std::size_t>(logic_.card());
  const Entrant *who = index < cards.size() ? contests_.entrant(cards[index].entrant) : nullptr;
  if (who == nullptr) {
    return;
  }
  const RegulationCheck &check = logic_.check();
  const PilotRecord *pilot = check.parts.pilot(who->pilot);
  const auto text = [&](const std::string &line, const std::array<float, 2> &at) {
    if (!line.empty()) {
      list.text(info.text_layer, layout_run(font_, glyph_run(font_, line), at[0], at[1]),
                info.text_colour);
    }
  };
  if (pilot != nullptr) {
    text(pilot->name, info.pilot);
    if (!pilot->name.empty()) {
      text(pilot->robot_name, info.partner);
    }
  }
  if (who->robot >= 0 && who->robot < static_cast<int>(check.garage.robots.size())) {
    text(check.garage.robots[static_cast<std::size_t>(who->robot)].name, info.robot);
  }
  text(pilot != nullptr && !pilot->team.empty() ? pilot->team : info.no_team, info.team);
  draw_amount(list, hud_.font, info.bounty, info.bounty_digits, cards[index].bounty,
              info.bounty_unit, info.text_colour, info.text_layer);
  draw_armour(list, cards[index].entrant);
  if (pilot == nullptr) {
    return;
  }
  for (const auto &[face, at] :
       {std::pair{&pilot->face, info.pilot_face}, {&pilot->robot_face, info.partner_face}}) {
    if (*face) {
      Sprite sprite = placed(**face, at);
      sprite.layer = info.face_layer;
      list.sprite(sprite);
    }
  }
}

// 0x013d07: the INFORMATION window pops open, then the contest's details
// and the way back.
void RumblingScreen::draw_contest_details(DrawList &list) const {
  const RumblingContestInfo &info = logic_.script().contest_info;
  if (!draw_pop_window(list, info.window, logic_.info_seconds())) {
    return;
  }
  draw_contest_info(list, font_, hud_.font, info.style, contests_, logic_.contest(logic_.area()));
  for (const Sprite &hint : info.close_hint) {
    list.sprite(hint);
  }
}

void RumblingScreen::draw(const Viewport &viewport) {
  draw_town(viewport, logic_.seconds(), logic_.black_alpha(), [this](DrawList &list) {
    const RumblingScript &script = logic_.script();
    draw_items(list);
    const float t = logic_.turn();
    if (t < 1.0F) {
      list.sprite(script.area_select.picture_frame, 1.0F - t);
    }
    const float fade = logic_.area_fade();
    if (fade < 1.0F && logic_.faded_area() != logic_.area()) {
      draw_area(list, logic_.faded_area(), 1.0F - fade);
    }
    draw_area(list, logic_.area(), fade < 1.0F ? fade : 1.0F);
    if (t < 1.0F) {
      draw_info(list);
    }
    if (t > 0.0F) {
      draw_menu(list);
    }
    if (logic_.view() == RumblingView::gates) {
      draw_gates(list);
    }
    if (logic_.view() == RumblingView::character_info) {
      draw_character_info(list);
    }
    if (logic_.view() == RumblingView::contest_info) {
      draw_contest_details(list);
    }
    notice_.draw(list);
    list.ticker(script.help_style, glyph_run(font_, logic_.help_text()), logic_.help_seconds());
  });
}

} // namespace phantomcrash
