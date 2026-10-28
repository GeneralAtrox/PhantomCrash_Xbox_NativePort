#include "desktop_slai_area.hpp"

#include "desktop_draw_list.hpp"
#include "desktop_schedule.hpp"
#include "desktop_shop_list.hpp"
#include "phantomcrash/money.hpp"

#include <algorithm>
#include <utility>

namespace phantomcrash {
namespace {

Sprite placed(Sprite sprite, const std::array<float, 2> &at) {
  sprite.position = at;
  return sprite;
}

float lerp(const float from, const float to, const float t) {
  return from + (to - from) * t;
}

// The art at `at`, `scale` times its size.
Sprite art(const std::string &image, const TexelRect &rect, const std::array<float, 2> &at,
           const float scale, const int layer) {
  Sprite sprite;
  sprite.image = image;
  sprite.rect = rect;
  sprite.position = at;
  sprite.size = std::array<float, 2>{(rect[2] - rect[0]) * scale, (rect[3] - rect[1]) * scale};
  sprite.layer = layer;
  return sprite;
}

} // namespace

SlaiAreaScreen::SlaiAreaScreen(TextureCache &textures, const GlyphFont &font, SlaiAreaSelect logic,
                               const MessageBoxStyle &box, const std::string &from,
                               const FrameStyle &frame, const std::vector<MusicCredit> &credits,
                               const QuestSave &quest, int &remembered_area)
    : TownScreen(textures, font, logic.rumbling(), from, frame, credits, quest),
      logic_(std::move(logic)), hud_(frame.hud), notice_(box, logic_.rumbling().message_text_at),
      remembered_area_(remembered_area) {}

void SlaiAreaScreen::tick(const InputFrame &input, const double tick_seconds) {
  logic_.update(tick_seconds, input);
  tick_frame(tick_seconds, logic_.faded_in());
  notice_.update(logic_.message(), false, 0, logic_.messages(), tick_seconds);
  if (logic_.done()) {
    remembered_area_ = logic_.area();
  }
}

std::vector<std::string> SlaiAreaScreen::take_sounds() {
  std::vector<std::string> sounds = logic_.take_sounds();
  for (std::string &name : notice_.take_sounds()) {
    sounds.push_back(std::move(name));
  }
  return sounds;
}

RumblingLook SlaiAreaScreen::look() const {
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

const SlaiRegulation *SlaiAreaScreen::rule(const int area) const {
  const auto &held = logic_.held(area);
  const SlaiRumbleData &data = logic_.data();
  const SlaiMatch *match = held ? slai_match(data.table, held->match) : nullptr;
  return match != nullptr ? slai_regulation(data.table, match->regulation) : nullptr;
}

// A city's emblem, its "<CITY> / DAY" (or NIGHT) label and its venue's name
// from its Wired sheet, where RUMBLING draws the area's logo.
void SlaiAreaScreen::draw_title(DrawList &list, const int area, const std::array<float, 2> &at,
                                const float alpha) const {
  const SlaiAreaSelectScript &script = logic_.script();
  const int column = logic_.city(area);
  if (column < 0 || column >= static_cast<int>(script.hub.city_art.size())) {
    return;
  }
  const SlaiCityArt &city = script.hub.city_art[static_cast<std::size_t>(column)];
  const SlaiAreaTitle &title = script.title;
  const auto from = [&at](const std::array<float, 2> &offset) {
    return std::array<float, 2>{at[0] + offset[0], at[1] + offset[1]};
  };
  const auto &box = title.emblem_box;
  const float width = city.emblem[2] - city.emblem[0];
  const float height = city.emblem[3] - city.emblem[1];
  if (width > 0.0F && height > 0.0F) {
    const float scale = std::min((box[2] - box[0]) / width, (box[3] - box[1]) / height);
    const std::array<float, 2> corner{box[0] + (box[2] - box[0] - width * scale) * 0.5F,
                                      box[1] + (box[3] - box[1] - height * scale) * 0.5F};
    list.sprite(art(city.image, city.emblem, from(corner), scale, title.layer), alpha);
  }
  const auto &held = logic_.held(area);
  const TexelRect &label = held && held->night ? city.night : city.day;
  list.sprite(art(city.image, label, from(title.label_at), title.label_scale, title.layer), alpha);
  if (city.venue[2] > city.venue[0]) {
    list.sprite(art(city.image, city.venue, from(title.venue_at), title.venue_scale, title.layer),
                alpha);
  }
}

// As RUMBLING's 0x015429: the city's blueprint in the picture frame, its
// title, and CLOSED over noise while the rota keeps it closed; turning to
// the sub-menu fades them and moves the title to the menu's top.
void SlaiAreaScreen::draw_area(DrawList &list, const int area, const float alpha) const {
  const RumblingScript &script = logic_.rumbling();
  const RumblingArea &shown = script.areas[static_cast<std::size_t>(area)];
  const float t = logic_.turn();
  const float stay = alpha * (1.0F - t);
  const CanvasPoint &to = script.menu.title_at;
  const std::array<float, 2> at{lerp(shown.title.position[0], to[0], t),
                                lerp(shown.title.position[1], to[1], t)};
  if (shown.exit) {
    list.sprite(shown.picture, stay);
    list.sprite(shown.label, stay);
    list.sprite(placed(shown.title, at), alpha);
    return;
  }
  list.sprite(logic_.script().picture_back, stay);
  list.sprite(shown.picture, stay);
  draw_title(list, area, at, alpha);
  if (!logic_.held(area)) {
    list.sprite(script.area_select.closed, stay);
    list.sprite(script.area_select.noise, stay * script.area_select.noise_alpha);
  }
}

// As RUMBLING's 0x015c44: the rumble's regulation and class, the money it
// needs and CLEAR once its record is cleared, sliding off to the right as
// the sub-menu opens.
void SlaiAreaScreen::draw_info(DrawList &list) const {
  const RumblingScript &script = logic_.rumbling();
  const RumblingAreaSelect &style = script.area_select;
  const float dx = script.menu.info_slide * logic_.turn();
  list.sprite(shifted(style.info_panel, dx, 0.0F));
  const SlaiRegulation *shown = rule(logic_.area());
  if (shown == nullptr) {
    return;
  }
  const auto right_text = [&](const std::string &text, const float y) {
    const std::vector<int> run = glyph_run(font_, text);
    list.text(style.text_layer,
              layout_run(font_, run, style.info_right + dx - run_width(font_, run), y),
              style.text_colour);
  };
  right_text(shown->name, style.contest_y);
  if (const SlaiHubBadge *badge = slai_hub_badge(logic_.script().hub, shown->id)) {
    right_text(badge->class_text, style.class_y);
  }
  NumberStyle fee = hud_.money;
  fee.right = {style.fee_at[0] + dx, style.fee_at[1]};
  fee.digits = style.fee_digits;
  fee.colour = style.text_colour;
  const std::int64_t needed = slai_entry_requirement(logic_.data().world, *shown);
  for (const Sprite &digit : money_sprites(hud_, fee, shown_ny(needed), style.text_layer)) {
    list.sprite(digit);
  }
  if (logic_.cleared(logic_.area())) {
    list.sprite(style.clear, 1.0F - logic_.turn());
  }
}

// As RUMBLING's 0x013d07: the INFORMATION window pops open with the
// rumble's details as Rumble S.L.A.I. gives them, and the way back.
void SlaiAreaScreen::draw_details(DrawList &list) const {
  const RumblingContestInfo &info = logic_.rumbling().contest_info;
  if (!draw_pop_window(list, info.window, logic_.info_seconds())) {
    return;
  }
  if (const SlaiRegulation *shown = rule(logic_.area())) {
    const SlaiHubBadge *badge = slai_hub_badge(logic_.script().hub, shown->id);
    draw_slai_rule_info(list, font_, hud_.font, info.style, logic_.data(), *shown,
                        badge != nullptr ? badge->class_text : std::string{});
  }
  for (const Sprite &hint : info.close_hint) {
    list.sprite(hint);
  }
}

void SlaiAreaScreen::draw(const Viewport &viewport) {
  draw_town(viewport, logic_.seconds(), logic_.black_alpha(), [this](DrawList &list) {
    const RumblingScript &script = logic_.rumbling();
    const RumblingLook shown = look();
    draw_rumbling_items(list, script, shown);
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
    const RumblingArea &area = script.areas[static_cast<std::size_t>(logic_.area())];
    if (t > 0.0F) {
      draw_rumbling_menu_items(list, script.menu, shown);
      static_cast<void>(draw_rumbling_map(list, script.menu, area, shown));
    }
    if (logic_.view() == RumblingView::gates) {
      draw_rumbling_gates(list, script.gates, area, shown);
    }
    if (logic_.view() == RumblingView::contest_info) {
      draw_details(list);
    }
    notice_.draw(list);
    list.ticker(script.help_style, glyph_run(font_, logic_.help_text()), logic_.help_seconds());
  });
}

} // namespace phantomcrash
