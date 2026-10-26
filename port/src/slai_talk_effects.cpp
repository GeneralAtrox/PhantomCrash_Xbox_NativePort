// The talk VM's screen effects: fades, the town glitch, the login screen,
// the town HUD, the panel's icons and the ambient loops (0x26C070, 0x271300,
// 0x2716F0, 0x271070, 0x271110).

#include "phantomcrash/slai_talk_scene.hpp"

#include <algorithm>
#include <cstdio>

namespace phantomcrash {
namespace {

constexpr int panel_alpha_step = 0x20;
constexpr int panel_fade_frames = 4;

// ambient_loop's objects: the first two play one anim, the third another;
// the second runs half as fast again.
constexpr int ambient_alpha_step = 0x10, ambient_alpha_full = 0x80;
constexpr int ambient_anim = 0x23, ambient_anim_last = 0x22;
constexpr float ambient_fast = 1.5F;

int alpha_of(const AnbObject &object) {
  return static_cast<int>(object.colour >> 24U);
}

void set_alpha(AnbObject &object, int alpha) {
  object.colour = (object.colour & 0x00FFFFFFU) | (static_cast<std::uint32_t>(alpha) << 24U);
}

// 0x271300's restores: glitch arg -> the cells shown again and the rate
// left after (0: unchanged).
struct GlitchRestore {
  int arg, first, last, rate;
};
constexpr std::array<GlitchRestore, 5> glitch_restores{{{0x0D, 0, 125, 2},
                                                        {0x0C, 125, 250, 2},
                                                        {0x0B, 250, 375, 3},
                                                        {0x0A, 375, 500, 0},
                                                        {0x04, 0, 500, 2}}};
// The glitch's other args set the text rate: 1 -> 10, 2 -> 20, 3 normal.
constexpr int glitch_rate_1 = 10;
constexpr int glitch_rate_2 = 20;
constexpr int glitch_broken_rate = 3; // once more than 100 cells are gone

} // namespace

bool SlaiTalkScene::exec_effect(int op, const std::vector<int> &args) {
  const int arg = args.empty() ? 0 : args[0];
  switch (op) {
  case slai_talk_op::fade_in: // 0x249880(2n) from black
    fade_ = arg > 0 ? 1.0F : 0.0F;
    fade_step_ = arg > 0 ? -1.0F / static_cast<float>(2 * arg) : 0.0F;
    return arg > 0;
  case slai_talk_op::fade_out: // 0x249720(2n, 0xFF) to black
    fade_step_ = arg > 0 ? 1.0F / static_cast<float>(2 * arg) : 0.0F;
    if (arg <= 0) {
      fade_ = 1.0F;
    }
    return arg > 0;
  case slai_talk_op::town_hud: // the town HUD's slide; the port has no S.L.A.I. HUD
    town_hud_ = arg;
    return false;
  case slai_talk_op::login_screen:
    login_frame_ = 0;
    login_percent_ = 0;
    return true;
  case slai_talk_op::glitch:
    if (arg == 0) {
      glitch_counter_ = 0;
      return true;
    }
    for (const auto &restore : glitch_restores) {
      if (restore.arg == arg) {
        for (int i = restore.first; i < restore.last; ++i) {
          glitch_hidden_.reset(static_cast<std::size_t>(i));
        }
        if (restore.rate != 0) {
          text_rate(restore.rate);
        }
        return false;
      }
    }
    if (arg == 1) {
      text_rate(glitch_rate_1);
    } else if (arg == 2) {
      text_rate(glitch_rate_2);
    } else if (arg == 3) {
      text_rate(refresh_rate);
    }
    return false;
  case slai_talk_op::panel_sprites: // 0x2716F0
    panel_fade_ = 0;
    if (arg < 0) {
      panel_fading_out_ = true;
      return true;
    }
    if (arg < static_cast<int>(panel_icons_.size())) {
      auto &sprite = panel_icons_[static_cast<std::size_t>(arg)];
      const auto index = static_cast<std::size_t>(arg);
      const auto position = index < table_.panel_sprites.size() ? table_.panel_sprites[index]
                                                                : std::array<int, 2>{256, 224};
      sprite = {icons_[index] >= 0 ? character_texture(icons_[index]) : "",
                0,
                0,
                64,
                64,
                static_cast<float>(position[0]),
                static_cast<float>(position[1])};
      sprite.alpha = 0.0F;
      sprite.visible = true;
      panel_fading_out_ = false;
    }
    return true;
  default:
    return false;
  }
}

bool SlaiTalkScene::poll_effect(int op) {
  switch (op) {
  case slai_talk_op::fade_in:
  case slai_talk_op::fade_out:
    return fade_step_ == 0.0F;
  case slai_talk_op::login_screen:
    return poll_login();
  case slai_talk_op::glitch:
    return poll_glitch();
  case slai_talk_op::panel_sprites:
    ++panel_fade_;
    for (auto &sprite : panel_icons_) {
      if (!sprite.visible) {
        continue;
      }
      if (panel_fading_out_) {
        sprite.alpha = std::max(sprite.alpha - panel_alpha_step, 0.0F);
        sprite.visible = sprite.alpha > 0.0F;
      } else {
        sprite.alpha = std::min(sprite.alpha + panel_alpha_step, 128.0F);
      }
    }
    return panel_fade_ >= panel_fade_frames;
  default:
    return true; // an op the port does not run yet ends at once
  }
}

// 0x271550: the town breaks up into hidden cells, slowly then faster.
bool SlaiTalkScene::poll_glitch() {
  const int before = glitch_counter_;
  if (glitch_counter_ < 100) {
    if ((frame_ & 0xF) == 0) {
      glitch_counter_ += 25;
    }
  } else if (glitch_counter_ < 200) {
    if ((frame_ & 7) == 0) {
      glitch_counter_ += 25;
    }
  } else if (glitch_counter_ < 300) {
    glitch_counter_ += 15;
  } else if (glitch_counter_ < 400) {
    glitch_counter_ += 20;
  } else {
    glitch_counter_ += 25;
  }
  if (glitch_counter_ > 100) {
    text_rate(glitch_broken_rate);
  }
  const bool done = glitch_counter_ >= glitch_cells;
  glitch_counter_ = std::min(glitch_counter_, glitch_cells);
  for (int i = before; i < glitch_counter_; ++i) {
    glitch_hidden_.set(static_cast<std::size_t>(i));
  }
  return done;
}

// 0x26C1F0: the percentage follows the (frame, value) curve; the screen
// closes after its last point.
bool SlaiTalkScene::poll_login() {
  ++login_frame_;
  const auto &curve = table_.login_progress;
  if (curve.empty() || login_frame_ > curve.back()[0]) {
    login_frame_ = -1;
    return true;
  }
  for (std::size_t i = 1; i < curve.size(); ++i) {
    if (login_frame_ <= curve[i][0]) {
      const auto &a = curve[i - 1];
      const auto &b = curve[i];
      const int span = std::max(1, b[0] - a[0]);
      login_percent_ = a[1] + (b[1] - a[1]) * (login_frame_ - a[0]) / span;
      break;
    }
  }
  return false;
}

// 0x271070: the old loop fades out over 30 frames as the new one fades in.
bool SlaiTalkScene::exec_ambient(int loop) {
  ambient_ = loop;
  for (auto &playing : loops_) {
    playing.step = -1;
  }
  const auto index = static_cast<std::size_t>(ambient_);
  if (ambient_ > 0 && index < table_.ambient_sounds.size() && table_.ambient_sounds[index] > 0) {
    std::array<char, 48> name{};
    std::snprintf(name.data(), name.size(), "slai/assets/sound/cue_%03d.wav",
                  table_.ambient_sounds[index]);
    loops_.push_back({name.data(), next_loop_id_++, 0, 1});
  }
  return true;
}

// 0x271110: the loop's objects fade in on its mask and out off it.
bool SlaiTalkScene::poll_ambient() {
  using namespace slai_talk_stage;
  const auto index = static_cast<std::size_t>(ambient_);
  const int mask = index < table_.ambient_objects.size() ? table_.ambient_objects[index] : 0;
  bool done = true;
  for (int i = 0; i < ambient_count; ++i) {
    auto &object = objects_[static_cast<std::size_t>(ambient + i)];
    if ((mask & (1 << i)) == 0) {
      const int alpha = alpha_of(object) - ambient_alpha_step;
      if (alpha < 0) {
        object.flags |= anb_flag::hidden;
      } else {
        set_alpha(object, alpha);
        done = false;
      }
    } else if (!object.has(anb_flag::hidden)) {
      const int alpha = alpha_of(object) + ambient_alpha_step;
      set_alpha(object, std::min(alpha, ambient_alpha_full));
      done = done && alpha >= ambient_alpha_full;
    } else {
      object.reset(false);
      object.set_anim(i < 2 ? ambient_anim : ambient_anim_last);
      set_alpha(object, 0);
      if (i == 1) {
        object.speed = ambient_fast;
      }
      object.flags = (object.flags | anb_flag::hold) & ~(anb_flag::hidden | anb_flag::ended);
      done = false;
    }
  }
  return done;
}

// Once a frame: the loops' 30-frame fades; a faded-out loop stops.
void SlaiTalkScene::step_loops() {
  for (auto &loop : loops_) {
    loop.fade = std::clamp(loop.fade + loop.step, 0, SlaiTalkLoop::fade_frames);
  }
  std::erase_if(loops_, [](const SlaiTalkLoop &loop) { return loop.step < 0 && loop.fade == 0; });
}

} // namespace phantomcrash
