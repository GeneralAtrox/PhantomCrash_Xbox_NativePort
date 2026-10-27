// The talk VM's joined cast and set pieces: the badges of the characters who
// have joined (FaceWork, TALK_JOIN_CHARA_MAX; 0x26EED0..0x26F3D0, 0x270DB0),
// the chip window over the portrait (pChipWindowChip; 0x26DC70..0x26DF00),
// the character window (pCharaWindowChip; 0x26F750..0x26FA50) and the
// partner.

#include "phantomcrash/slai_talk_scene.hpp"

#include <algorithm>
#include <stdexcept>

namespace phantomcrash {
namespace {

constexpr int alpha_step = 0x20, alpha_full = 0x80;
constexpr unsigned play_clear = anb_flag::hidden | anb_flag::no_loop | anb_flag::ended; // 0x83

// badge: the frame's anims, and the name and face placed from the slot.
constexpr int badge_frame_in = 0x0B, badge_frame_out = 0x0C;
constexpr float badge_name_dx = 0xD0, badge_name_w = 16.0F, badge_name_h = 17.0F;
constexpr std::uint32_t badge_name_colour = 0x00805E22U; // 0x207510's, from alpha 0
constexpr float badge_face_dx = 0xEA, badge_face_y = 0x10A;
constexpr float face_u = 4.0F, face_w = 56.0F, face_h = 64.0F;
constexpr int face_grey = 0x40;
// The badges' petal lights sit on HAVEN's first four petals (0x4EC4D0).
constexpr float petal_step = 30.0F, petal_start = 7.0F, petal_radius = 169.0F;

// The chip and character windows (TAL01) and the shade behind them.
constexpr int chip_window_in = 0x1D, chip_window_out = 0x1E, chip_window_keep = 0x1F;
constexpr int shade_in = 0x1B, shade_out = 0x1C;
constexpr int character_window_in = 0x24, character_window_out = 0x25;
constexpr int se_chip_window_in = 0x2A, se_chip_window_out = 0x2B;
constexpr float window_x = 256.0F, window_y = 224.0F;

constexpr std::uint64_t bit(int op) {
  return std::uint64_t{1} << op;
}

int alpha_of(const AnbObject &object) {
  return static_cast<int>(object.colour >> 24U);
}

void set_alpha(AnbObject &object, int alpha) {
  object.colour = (object.colour & 0x00FFFFFFU) | (static_cast<std::uint32_t>(alpha) << 24U);
}

void set_alpha(SlaiTalkText &text, int alpha) {
  text.colour = (text.colour & 0x00FFFFFFU) | (static_cast<std::uint32_t>(alpha) << 24U);
}

std::vector<std::uint16_t> name_glyphs(const std::string &name) {
  try {
    return slai_talk_glyphs(name);
  } catch (const std::runtime_error &) {
    return {};
  }
}

} // namespace

// 0x4EC4D0: the petal lights round the first four petals start hidden; the
// focus glow adds (the sprite's +0x74 mode 1, GS ALPHA 0x48).
void SlaiTalkScene::make_badge_petals() {
  using namespace slai_talk_stage;
  for (const int first : {focus_glow, lit_petal, dim_petal}) {
    place_ring({first, badge_count, petal_step, petal_start, petal_radius});
    for (int i = 0; i < badge_count; ++i) {
      auto &sprite = sprites_[static_cast<std::size_t>(first + i)];
      sprite.visible = false;
      sprite.alpha = static_cast<float>(alpha_full);
      sprite.blend = first == focus_glow ? AnbBlend::additive : AnbBlend::alpha;
    }
  }
}

bool SlaiTalkScene::exec_join(int op, const std::vector<int> &args) {
  const auto arg = [&args](std::size_t i) { return i < args.size() ? args[i] : 0; };
  auto &shade = objects_[slai_talk_stage::chip_shade];
  switch (op) {
  case slai_talk_op::badge:
    return exec_badge(arg(0), arg(1), arg(2) != 0);
  case slai_talk_op::badge_remove:
    return exec_badge_remove(arg(0));
  case slai_talk_op::badge_focus: // 0x270DB0
    focus_ = arg(0);
    return true;
  case slai_talk_op::event_portrait: // 0x26DC70: a character's chip window
    icon_texture_ = character_texture(arg(0));
    chip_window_.x = window_x;
    chip_window_.y = window_y;
    chip_window_.play(chip_window_in, anb_flag::hold, play_clear);
    shade.play(shade_in, anb_flag::hold, play_clear);
    shade.colour = gs_white;
    set_focus(arg(0));
    sound(se_chip_window_in);
    return true;
  case slai_talk_op::event_portrait_close: // 0x26DE50
    chip_window_.play(chip_window_out, anb_flag::hold, play_clear);
    shade.reset(false);
    shade.set_anim(shade_out);
    shade.flags &= ~(play_clear | anb_flag::hold);
    objects_[slai_talk_stage::nameplate].flags |= anb_flag::hidden;
    set_focus(portrait_.character);
    sound(se_chip_window_out);
    return true;
  case slai_talk_op::event_image: // 0x26F750
    open_character_window(arg(0));
    return true;
  case slai_talk_op::event_image_open: // 0x26F8B0
    close_character_window();
    return true;
  case slai_talk_op::event_image_close: // 0x26F9B0: the chip window stays as the character's
    chip_ = chip_window_;
    chip_window_.flags |= anb_flag::hidden;
    chip_.play(chip_window_keep, anb_flag::hold, play_clear);
    objects_[slai_talk_stage::nameplate].flags |= anb_flag::hidden;
    return true;
  case slai_talk_op::ambient_loop:
    return exec_ambient(arg(0));
  // Without a partner chip (VM+0x770 is 0) the partner ops are the
  // Knocker's (0x2701A0, 0x270520, 0x270AD0) and partner_path and
  // partner_path_stop do nothing.
  case slai_talk_op::partner_in:
    return selector_ == 0 ? exec_cast(slai_talk_op::knocker_in, args) : exec_partner(op, args);
  case slai_talk_op::partner_out:
    return selector_ == 0 ? exec_cast(slai_talk_op::knocker_out, args) : exec_partner(op, args);
  case slai_talk_op::knocker_only_pose:
    return selector_ == 0 && exec_cast(slai_talk_op::knocker_pose, args);
  case slai_talk_op::partner_path:
  case slai_talk_op::partner_path_stop:
    return selector_ != 0 && exec_partner(op, args);
  default: // the unloads: files load as drawn here
    return false;
  }
}

bool SlaiTalkScene::poll_join(int op) {
  auto &shade = objects_[slai_talk_stage::chip_shade];
  switch (op) {
  case slai_talk_op::badge:
    return poll_badge_in();
  case slai_talk_op::badge_remove:
    return poll_badge_out();
  case slai_talk_op::badge_focus:
    return poll_focus();
  case slai_talk_op::event_portrait: // 0x26DDF0
    return chip_window_.has(anb_flag::ended) && shade.has(anb_flag::ended);
  case slai_talk_op::event_portrait_close: { // 0x26DF00
    bool done = true;
    if (chip_window_.visible()) {
      if (chip_window_.has(anb_flag::ended)) {
        chip_window_.flags |= anb_flag::hidden;
      } else {
        done = false;
      }
    }
    if (!shade.has(anb_flag::ended)) {
      return false;
    }
    shade.flags |= anb_flag::hidden;
    return done;
  }
  case slai_talk_op::event_image: // 0x26F780
    return chip_.has(anb_flag::ended);
  case slai_talk_op::event_image_open: // 0x26F8D0: the window goes when its anim ends
    if (!chip_.has(anb_flag::ended)) {
      return false;
    }
    chip_.flags |= anb_flag::hidden;
    return true;
  case slai_talk_op::event_image_close: { // 0x26FA50: the shade fades
    const int alpha = alpha_of(shade) - alpha_step;
    if (alpha < 1) {
      shade.flags |= anb_flag::hidden;
      return chip_.has(anb_flag::ended);
    }
    set_alpha(shade, alpha);
    return false;
  }
  case slai_talk_op::ambient_loop:
    return poll_ambient();
  case slai_talk_op::partner_in:
    return selector_ == 0 ? poll_cast(slai_talk_op::knocker_in) : poll_partner(op);
  case slai_talk_op::partner_out:
    return selector_ == 0 ? poll_cast(slai_talk_op::knocker_out) : poll_partner(op);
  case slai_talk_op::partner_path:
    return poll_partner(op);
  case slai_talk_op::knocker_only_pose:
    return poll_cast(slai_talk_op::knocker_pose);
  default:
    return true;
  }
}

// 0x26EED0: a character joins in a slot: its frame, name and face fade in.
// The same character again only renames it.
bool SlaiTalkScene::exec_badge(int character, int slot, bool unknown) {
  using namespace slai_talk_stage;
  if (slot < 0 || slot >= badge_count ||
      static_cast<std::size_t>(slot) >= table_.badge_slots.size()) {
    return false;
  }
  const auto *row = table_.character(character);
  const auto glyphs = name_glyphs(unknown          ? table_.badge_unknown_name
                                  : row != nullptr ? row->name
                                                   : std::string{});
  auto &badge = badges_[static_cast<std::size_t>(slot)];
  if (badge.character == character) {
    badge.name.glyphs = glyphs;
    return false;
  }
  const auto [x, y] = table_.badge_slots[static_cast<std::size_t>(slot)];
  badge = {};
  badge.character = character;
  badge.shown = true;
  sprites_[static_cast<std::size_t>(dim_petal + slot)].visible = true;
  auto &frame = objects_[static_cast<std::size_t>(badge_frames + slot)];
  frame.x = static_cast<float>(x + 256);
  frame.y = static_cast<float>(224 - y);
  frame.play(badge_frame_in, anb_flag::hold, play_clear);
  badge.name = {glyphs,
                static_cast<float>(x) + badge_name_dx,
                static_cast<float>(224 - y),
                badge_name_w,
                badge_name_h,
                badge_name_colour};
  badge.face.texture = character_texture(character);
  badge.face.u = face_u;
  badge.face.w = face_w;
  badge.face.h = face_h;
  badge.face.x = static_cast<float>(x) + badge_face_dx;
  badge.face.y = badge_face_y - static_cast<float>(y);
  badge.face.colour = 0x80000000U | face_grey * 0x010101U;
  badge.face.alpha = 0.0F;
  badge.face.visible = true;
  return true;
}

// 0x26F1B0: faces fade in; each name once its frame has opened.
bool SlaiTalkScene::poll_badge_in() {
  bool done = true;
  for (std::size_t i = 0; i < badges_.size(); ++i) {
    auto &badge = badges_[i];
    if (badge.character == 0 || !badge.shown) {
      continue;
    }
    badge.face.alpha = std::min<float>(badge.face.alpha + alpha_step, alpha_full);
    done = done && badge.face.alpha >= alpha_full;
    if (!objects_[slai_talk_stage::badge_frames + i].has(anb_flag::ended)) {
      done = false;
      continue;
    }
    const int alpha = std::min(static_cast<int>(badge.name.colour >> 24U) + alpha_step, alpha_full);
    set_alpha(badge.name, alpha);
    done = done && alpha >= alpha_full;
  }
  return done;
}

// 0x26F2F0: the frame closes and the name goes at once.
bool SlaiTalkScene::exec_badge_remove(int character) {
  const auto found = std::ranges::find(badges_, character, &SlaiTalkBadge::character);
  if (character == 0 || found == badges_.end()) {
    return false;
  }
  const auto slot = static_cast<std::size_t>(found - badges_.begin());
  auto &frame = objects_[slai_talk_stage::badge_frames + slot];
  frame.reset(false);
  frame.set_anim(badge_frame_out);
  found->name = {};
  found->shown = false;
  return true;
}

// 0x26F3D0: the face fades out, then the slot empties.
bool SlaiTalkScene::poll_badge_out() {
  bool done = true;
  for (std::size_t i = 0; i < badges_.size(); ++i) {
    auto &badge = badges_[i];
    const auto &frame = objects_[slai_talk_stage::badge_frames + i];
    if (badge.character == 0 || badge.shown || frame.has(anb_flag::hidden)) {
      continue;
    }
    badge.face.alpha = std::max(badge.face.alpha - alpha_step, 0.0F);
    if (badge.face.alpha == 0.0F && frame.has(anb_flag::ended)) {
      sprites_[slai_talk_stage::dim_petal + i].visible = false;
      badge = {};
    } else {
      done = false;
    }
  }
  return done;
}

// 0x4EF850: light up a character's badge (0: none).
void SlaiTalkScene::set_focus(int character) {
  focus_ = character;
  pending_ |= bit(slai_talk_op::badge_focus);
}

// 0x270DE0: the focused badge's petal lights and glows, the rest dim.
bool SlaiTalkScene::poll_focus() {
  using namespace slai_talk_stage;
  bool done = true;
  const auto fade = [&done](SlaiTalkSprite &sprite, int step) {
    const float alpha = sprite.alpha + static_cast<float>(step);
    if (step > 0) {
      sprite.visible = true;
      sprite.alpha = std::min<float>(alpha, alpha_full);
      done = done && alpha >= alpha_full;
    } else if (!sprite.visible) {
      return 0;
    } else if (alpha < 0.0F) {
      sprite.visible = false;
      return 0;
    } else {
      sprite.alpha = alpha;
      done = false;
    }
    return static_cast<int>(sprite.alpha);
  };
  for (std::size_t i = 0; i < badges_.size(); ++i) {
    auto &badge = badges_[i];
    if (badge.character == 0) {
      continue;
    }
    const bool focused = badge.character == focus_;
    fade(sprites_[dim_petal + i], focused ? -alpha_step : alpha_step);
    const int lit = fade(sprites_[lit_petal + i], focused ? alpha_step : -alpha_step);
    const int grey = lit / 2 + face_grey;
    badge.face.colour = 0x80000000U | static_cast<std::uint32_t>(grey) * 0x010101U;
    badge.face.alpha = static_cast<float>(alpha_full);
    sprites_[focus_glow + i].visible = focused;
  }
  return done;
}

// 0x26F7B0: a chip's character window opens. The original asserts the old
// one has gone; here a window still closing gives way to the new.
void SlaiTalkScene::open_character_window(int chip) {
  pending_ &= ~bit(slai_talk_op::event_image_open);
  icon_texture_ = character_texture(chip);
  chip_.x = window_x;
  chip_.y = window_y;
  chip_.play(character_window_in, anb_flag::hold, play_clear);
}

// 0x26F930: the character window closes, and the nameplate with it.
void SlaiTalkScene::close_character_window() {
  chip_.play(character_window_out, anb_flag::hold, play_clear);
  objects_[slai_talk_stage::nameplate].flags |= anb_flag::hidden;
}

} // namespace phantomcrash
