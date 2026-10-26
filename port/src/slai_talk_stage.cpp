// The talk VM's stage: the HAVEN backdrop and rings, the portrait panel and
// its voice meter, the news frame and pictures (0x26AFC0..0x26DC10).

#include "phantomcrash/slai_talk_scene.hpp"

#include <algorithm>
#include <cmath>
#include <numbers>

namespace phantomcrash {
namespace {

constexpr float gs_alpha_one = 128.0F;
constexpr int alpha_step = 0x20;
constexpr float haven_squash = 0.88F; // the rings' x scale (0x3F6147AE)
constexpr float haven_centre_x = 256.0F;
constexpr float haven_centre_y = 258.0F;

const std::string tal01 = "talk/common/tal01.json";
const std::string tal02 = "talk/common/tal02.png";
const std::string tal01_picture = "talk/common/tal01.png";
const std::string backdrop_picture = "talk/common/tal_bg01.png";

// VM+0xC0's sprites by first index, count and TAL01/TAL02 rectangle (0x4EC4D0).
struct SpriteGroup {
  int first, count;
  const std::string *texture;
  float u, v, w, h;
};
const std::array<SpriteGroup, 8> sprite_groups{{
    {slai_talk_stage::petals, 12, &tal02, 0, 0, 128, 160},
    {12, 4, &tal02, 256, 0, 128, 160},
    {16, 4, &tal02, 256, 0, 128, 160},
    {20, 4, &tal02, 128, 0, 128, 160},
    {slai_talk_stage::ring_1, 6, &tal02, 208, 304, 304, 72},
    {slai_talk_stage::ring_2, 8, &tal02, 208, 232, 304, 72},
    {slai_talk_stage::ring_3, 6, &tal02, 208, 160, 304, 72},
    {slai_talk_stage::meters, 7, &tal01_picture, 320, 9, 8, 46},
}};

// haven_open_2/3's placements: (first sprite, count, degrees apart, first
// angle, radius), each on an ellipse squashed to 0.88 across.
constexpr SlaiTalkRing petal_ring{slai_talk_stage::petals, 12, 30.0F, 7.0F, 169.0F};
constexpr std::array<SlaiTalkRing, 3> rings{{{slai_talk_stage::ring_1, 6, 60.0F, 15.0F, 245.0F},
                                             {slai_talk_stage::ring_2, 8, 45.0F, 50.0F, 256.0F},
                                             {slai_talk_stage::ring_3, 6, 60.0F, 30.0F, 259.0F}}};

int clamp_alpha(int value) {
  return std::clamp(value, 0, 0x80);
}

void set_alpha(AnbObject &object, int alpha) {
  object.colour = (object.colour & 0x00FFFFFFU) | (static_cast<std::uint32_t>(alpha) << 24U);
}

int alpha_of(const AnbObject &object) {
  return static_cast<int>(object.colour >> 24U);
}

void haven_play(AnbObject &object, int anim, unsigned clear) {
  object.play(anim, anb_flag::hold, clear);
  object.scale_x = haven_squash;
  object.scale_y = 1.0F;
}

} // namespace

void SlaiTalkScene::make_stage() {
  const AnbBank *common = bank(tal01);
  for (auto &object : objects_) {
    object = {};
    object.bank = common;
  }
  for (auto &window : windows_) {
    window.frame.bank = common;
    window.layer.bank = common;
  }
  chip_.bank = common;
  chip_window_.bank = common;
  sprites_.assign(slai_talk_stage::sprite_count, {});
  for (const auto &group : sprite_groups) {
    for (int i = 0; i < group.count; ++i) {
      auto &sprite = sprites_[static_cast<std::size_t>(group.first + i)];
      sprite.texture = *group.texture;
      sprite.u = group.u;
      sprite.v = group.v;
      sprite.w = group.w;
      sprite.h = group.h;
    }
  }
  backdrop_ = {backdrop_picture, 0, 0, 512, 448};
  make_badge_petals();
}

void SlaiTalkScene::place_ring(const SlaiTalkRing &ring) {
  for (int i = 0; i < ring.count; ++i) {
    float angle =
        (static_cast<float>(i) * ring.step + ring.start) * std::numbers::pi_v<float> / 180.0F;
    if (angle > std::numbers::pi_v<float>) {
      angle -= 2.0F * std::numbers::pi_v<float>;
    }
    auto &sprite = sprites_[static_cast<std::size_t>(ring.first + i)];
    sprite.visible = true;
    sprite.alpha = 0.0F;
    sprite.rotation = angle;
    sprite.scale_x = sprite.scale_y = haven_squash;
    sprite.x = haven_centre_x + std::sin(angle) * ring.radius * haven_squash;
    sprite.y = haven_centre_y - std::cos(angle) * ring.radius;
  }
}

void SlaiTalkScene::ring_alpha(const SlaiTalkRing &ring, int alpha) {
  for (int i = 0; i < ring.count; ++i) {
    sprites_[static_cast<std::size_t>(ring.first + i)].alpha = static_cast<float>(alpha);
  }
}

bool SlaiTalkScene::exec_stage(int op, const std::vector<int> &args) {
  const int arg = args.empty() ? 0 : args[0];
  const auto object = [this](int index) -> AnbObject & {
    return objects_[static_cast<std::size_t>(index)];
  };
  switch (op) {
  case slai_talk_op::backdrop_in: // 0x26AFC0
    backdrop_steps_ = arg;
    backdrop_.visible = true;
    backdrop_.alpha = arg != 0 ? 0.0F : gs_alpha_one;
    return arg != 0;
  case slai_talk_op::haven_open: // 0x26B170
    haven_play(object(slai_talk_stage::haven_a), 6, 0x83);
    haven_play(object(slai_talk_stage::haven_e), 8, 0x83);
    object(slai_talk_stage::haven_c).x = 256.0F;
    object(slai_talk_stage::haven_c).y = 258.0F;
    haven_play(object(slai_talk_stage::haven_c), 0x20, 0x83);
    return true;
  case slai_talk_op::haven_open_2: // 0x26B3B0
    place_ring(petal_ring);
    counter_a_ = 0;
    return true;
  case slai_talk_op::haven_open_3: // 0x26B660
    for (const auto &ring : rings) {
      place_ring(ring);
    }
    counter_b_ = 0;
    return true;
  case slai_talk_op::haven_close: // 0x26BCE0
    object(slai_talk_stage::haven_b).flags |= anb_flag::hidden;
    object(slai_talk_stage::haven_d).flags |= anb_flag::hidden;
    object(slai_talk_stage::haven_a).play(7, 0, 0x85);
    object(slai_talk_stage::haven_e).play(9, 0, 0x85);
    object(slai_talk_stage::haven_c).play(0x21, 0, 0x85);
    return true;
  case slai_talk_op::rings_out: // 0x26BE20
    counter_a_ = 0;
    return true;
  case slai_talk_op::rings_out_2: // 0x26BEE0
    counter_b_ = 0;
    return true;
  case slai_talk_op::panel_open: // 0x26C670
    object(slai_talk_stage::panel_a).play(0);
    object(slai_talk_stage::panel_e).play(2);
    object(slai_talk_stage::panel_b).play(0x0D);
    sound(0x2A);
    return true;
  case slai_talk_op::panel_close: // 0x26C850
    object(slai_talk_stage::panel_a).play(1, 0, 0x87);
    object(slai_talk_stage::panel_e).play(3, 0, 0x87);
    for (const int index :
         {slai_talk_stage::panel_b, slai_talk_stage::panel_c, slai_talk_stage::panel_d}) {
      object(index).flags |= anb_flag::hidden;
    }
    for (int i = 0; i < 7; ++i) {
      sprites_[static_cast<std::size_t>(slai_talk_stage::meters + i)].visible = false;
    }
    sound(0x2B);
    return true;
  case slai_talk_op::news_open: // 0x26D0F0
    object(slai_talk_stage::news_frame).play(0x26);
    set_alpha(object(slai_talk_stage::news_frame), 0x80);
    background_ = background_next_ = 1;
    sound(0x1F);
    return true;
  case slai_talk_op::news_close:
    object(slai_talk_stage::news_frame).play(0x27, 0, 0x87);
    object(slai_talk_stage::news_anim).flags |= anb_flag::hidden;
    background_ = background_next_ = 0;
    sound(0x1F);
    return true;
  case slai_talk_op::background:
    return exec_background(arg);
  default:
    return exec_effect(op, args);
  }
}

// 0x26D5C0: 0 none, 1 the news frame, 2.. a picture (news_pictures).
bool SlaiTalkScene::exec_background(int next) {
  background_next_ = next;
  if (next == background_) {
    return false;
  }
  if (next > 1) {
    const auto index = static_cast<std::size_t>(next);
    picture_ = {index < table_.news_pictures.size() ? table_.news_pictures[index] : "",
                0,
                0,
                256,
                256,
                256.0F,
                169.0F};
    picture_.visible = true;
    if (background_ > 1) {
      background_ = next; // one picture straight to the next
      return false;
    }
    picture_.alpha = 0.0F;
  }
  sound(0x2C);
  return true;
}

bool SlaiTalkScene::poll_background() {
  auto &frame = objects_[slai_talk_stage::news_frame];
  bool done = true;
  if (background_next_ == 1) {
    frame.flags &= ~anb_flag::hidden;
    set_alpha(frame, clamp_alpha(alpha_of(frame) + alpha_step));
    done = done && alpha_of(frame) >= 0x80;
  } else if (background_next_ > 1) {
    picture_.alpha = std::min(picture_.alpha + alpha_step, gs_alpha_one);
    done = done && picture_.alpha >= gs_alpha_one;
  }
  if (background_ == 1) {
    objects_[slai_talk_stage::news_anim].flags |= anb_flag::hidden;
    set_alpha(frame, clamp_alpha(alpha_of(frame) - alpha_step));
    if (alpha_of(frame) == 0) {
      frame.flags |= anb_flag::hidden;
    }
    done = done && alpha_of(frame) == 0;
  } else if (background_ > 1) {
    picture_.alpha = std::max(picture_.alpha - alpha_step, 0.0F);
    picture_.visible = picture_.alpha > 0.0F;
    done = done && !picture_.visible;
  }
  if (done) {
    background_ = background_next_;
    if (background_ == 1) {
      objects_[slai_talk_stage::news_anim].play(0x28);
    }
  }
  return done;
}

bool SlaiTalkScene::poll_stage(int op) {
  const auto object = [this](int index) -> AnbObject & {
    return objects_[static_cast<std::size_t>(index)];
  };
  switch (op) {
  case slai_talk_op::backdrop_in: // 0x26B0D0
    if (--backdrop_steps_ > 0) {
      backdrop_.alpha = static_cast<float>(
          (static_cast<int>(backdrop_.alpha) * backdrop_steps_ + 0x80) / (backdrop_steps_ + 1));
      return false;
    }
    backdrop_.alpha = gs_alpha_one;
    return true;
  case slai_talk_op::haven_open: // 0x26B2C0
    if (!object(slai_talk_stage::haven_a).has(anb_flag::ended)) {
      return false;
    }
    haven_play(object(slai_talk_stage::haven_b), 0x13, 0x81);
    haven_play(object(slai_talk_stage::haven_d), 0x14, 0x81);
    return true;
  case slai_talk_op::haven_open_2: { // 0x26B5B0
    ++counter_a_;
    int alpha = 0;
    for (int i = 0; i < petal_ring.count; ++i) {
      alpha = clamp_alpha((counter_a_ - i) * alpha_step);
      sprites_[static_cast<std::size_t>(petal_ring.first + i)].alpha = static_cast<float>(alpha);
    }
    return alpha >= 0x80;
  }
  case slai_talk_op::haven_open_3: // 0x26BB90
    ++counter_b_;
    ring_alpha(rings[1], clamp_alpha(counter_b_ * alpha_step));
    if ((counter_b_ - 5) * alpha_step <= 0) {
      return false;
    }
    ring_alpha(rings[0], clamp_alpha((counter_b_ - 5) * alpha_step));
    if ((counter_b_ - 10) * alpha_step <= 0) {
      return false;
    }
    ring_alpha(rings[2], clamp_alpha((counter_b_ - 10) * alpha_step));
    return (counter_b_ - 10) * alpha_step >= 0x80;
  case slai_talk_op::haven_close: // 0x26BDB0
    return object(slai_talk_stage::haven_a).has(anb_flag::ended) &&
           object(slai_talk_stage::haven_e).has(anb_flag::ended) &&
           object(slai_talk_stage::haven_c).has(anb_flag::ended);
  case slai_talk_op::rings_out: { // 0x26BE30
    ++counter_a_;
    int alpha = 0;
    for (int i = 0; i < petal_ring.count; ++i) {
      alpha = clamp_alpha(0x80 - (counter_a_ - i) * alpha_step);
      sprites_[static_cast<std::size_t>(petal_ring.first + i)].alpha = static_cast<float>(alpha);
    }
    return alpha < 1;
  }
  case slai_talk_op::rings_out_2: { // 0x26BEF0
    ++counter_b_;
    ring_alpha(rings[2], clamp_alpha(0x80 - counter_b_ * alpha_step));
    const int second = 0x80 - (counter_b_ - 5) * alpha_step;
    if (second >= 0x80) {
      return false;
    }
    ring_alpha(rings[0], clamp_alpha(second));
    const int third = 0x80 - (counter_b_ - 10) * alpha_step;
    if (third >= 0x80) {
      return false;
    }
    ring_alpha(rings[1], clamp_alpha(third));
    return third < 1;
  }
  case slai_talk_op::panel_open:
    if (!object(slai_talk_stage::panel_e).has(anb_flag::ended)) {
      return false;
    }
    object(slai_talk_stage::panel_c).play(0x11, anb_flag::hold, 0x81);
    object(slai_talk_stage::panel_d).play(0x12, anb_flag::hold, 0x81);
    for (int i = 0; i < 7; ++i) {
      sprites_[static_cast<std::size_t>(slai_talk_stage::meters + i)].visible = true;
    }
    return true;
  case slai_talk_op::panel_close:
    return object(slai_talk_stage::panel_a).has(anb_flag::hidden) &&
           object(slai_talk_stage::panel_e).has(anb_flag::hidden);
  case slai_talk_op::news_open:
    if (!object(slai_talk_stage::news_frame).has(anb_flag::ended)) {
      return false;
    }
    object(slai_talk_stage::news_anim).play(0x28);
    return true;
  case slai_talk_op::news_close:
    return object(slai_talk_stage::news_frame).has(anb_flag::hidden);
  case slai_talk_op::background:
    return poll_background();
  default:
    return poll_effect(op);
  }
}

// 0x4ECBF0: the meter's bars swing while the panel shows, harder while a
// portrait speaks.
void SlaiTalkScene::step_meter() {
  if (!sprites_[slai_talk_stage::meters].visible || table_.voice_meter.empty()) {
    return;
  }
  std::size_t set = 0;
  const bool speaking =
      (pending_ >> slai_talk_op::say & 1U) != 0 && windows_[0].open && windows_[0].style == 0;
  if (speaking && talk_sounds_ >= 0 &&
      static_cast<std::size_t>(talk_sounds_) < table_.voice_meter.size()) {
    set = static_cast<std::size_t>(talk_sounds_);
  }
  const auto &bars = table_.voice_meter[set];
  constexpr float meter_step = 0.1F;
  for (std::size_t i = 0; i < meter_levels_.size() && i < bars.size(); ++i) {
    const auto &bar = bars[i];
    const float turn = static_cast<float>(meter_frame_ % bar.period) * 360.0F /
                       static_cast<float>(bar.period) * std::numbers::pi_v<float> / 180.0F;
    const float target =
        bar.low + (bar.high - bar.low) * (0.5F + 0.5F * std::sin(bar.phase + turn));
    float &level = meter_levels_[i];
    if (level > target + meter_step) {
      level -= meter_step;
    } else if (level < target - meter_step) {
      level += meter_step;
    } else {
      level = target;
    }
    level = std::clamp(level, 0.0F, 1.0F);
    auto &sprite = sprites_[slai_talk_stage::meters + i];
    sprite.scale_y = level;
    sprite.x = 26.0F + 8.0F * static_cast<float>(i);
    sprite.y = 56.0F + 46.0F * (0.5F - 0.5F * level);
  }
  ++meter_frame_;
}

} // namespace phantomcrash
