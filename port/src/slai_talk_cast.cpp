// The talk VM's cast: the Knocker (knocker.cpp; 0x2701A0..0x2706F0,
// 0x4EA6C0), portraits (0x26F510, 0x4EF1C0) and their talk and blink
// animations (0x4ED590, 0x4ED6F0).

#include "phantomcrash/slai_talk_scene.hpp"

#include <algorithm>
#include <cstdio>

namespace phantomcrash {
namespace {

// knocker_in's stand-in places ('+' scenes, town scenes, town with the HUD
// raised), in the Knocker's own frame: x from the centre, y up.
constexpr int knocker_talk_x = 0xA4, knocker_talk_y = -45;
constexpr int knocker_town_x = 0xA1, knocker_town_y = -108, knocker_town_raised_y = -68;
constexpr int knocker_run_start = -300, knocker_run_step = 40;
constexpr int knocker_file_in = 0x13, knocker_file_out = 0x14, knocker_file_news = 0x0F,
              knocker_file_run = 8, knocker_file_talk = 6, knocker_file_point = 5;
constexpr int knocker_state_news = 1, knocker_state_point = 3;

constexpr float portrait_x = 125.0F, portrait_y = 189.0F;
constexpr int portrait_out_frames = 8, portrait_in_end = 21, crossfade_step = 8;
constexpr int mouth_last_character = 0x1C, eyes_last_character = 0x17;

std::string hex_name(const std::string &folder, int value, const char *extension) {
  std::array<char, 16> digits{};
  std::snprintf(digits.data(), digits.size(), "%x", value);
  return "talk/" + folder + "/" + folder + digits.data() + extension;
}

void set_alpha(AnbObject &object, int alpha) {
  object.colour = (object.colour & 0x00FFFFFFU) |
                  (static_cast<std::uint32_t>(std::clamp(alpha, 0, 0x80)) << 24U);
}

} // namespace

// 0x4EA6C0: the Knocker's object anew with one file's animation.
void SlaiTalkScene::knocker_play(int file, int anim) {
  std::array<char, 32> name{};
  std::snprintf(name.data(), name.size(), "talk/de000/de%04x.json", file);
  knocker_ = {};
  knocker_.bank = bank(name.data());
  knocker_file_ = file;
  knocker_texture_ = static_cast<std::size_t>(file) < table_.knocker_textures.size()
                         ? table_.knocker_textures[static_cast<std::size_t>(file)]
                         : std::string{};
  knocker_.x = static_cast<float>(knocker_x_ + 256);
  knocker_.y = static_cast<float>(224 - knocker_y_);
  knocker_.play(anim, anb_flag::hold, anb_flag::hidden | anb_flag::ended);
  if (file == knocker_file_point) {
    knocker_state_ = anim == 1 || anim == 2 ? knocker_state_point : 0;
  } else if (file != knocker_file_run && file != knocker_file_news) {
    knocker_state_ = 0;
  }
}

void SlaiTalkScene::knocker_place(int x, int y) {
  knocker_x_ = x;
  knocker_y_ = y;
  knocker_.x = static_cast<float>(x + 256);
  knocker_.y = static_cast<float>(224 - y);
}

bool SlaiTalkScene::exec_cast(int op, const std::vector<int> &args) {
  const auto arg = [&args](std::size_t i) { return i < args.size() ? args[i] : 0; };
  switch (op) {
  case slai_talk_op::knocker_in: { // 0x2701A0
    knocker_state_ = arg(0);
    knocker_phase_ = 0;
    const bool town = mode_ == SlaiTalkMode::town;
    const int x = town ? knocker_town_x : knocker_talk_x;
    const int y = !town ? knocker_talk_y : town_hud_ == 2 ? knocker_town_raised_y : knocker_town_y;
    if (arg(0) == 2) {
      knocker_target_x_ = x;
      knocker_play(knocker_file_run, 0);
      knocker_place(knocker_run_start, y);
    } else {
      knocker_place(x, y);
      knocker_play(arg(0) == 1 ? knocker_file_news : knocker_file_in, 0);
    }
    return true;
  }
  case slai_talk_op::knocker_out: // 0x270520
    knocker_state_ = arg(0);
    if (arg(0) == 1) {
      knocker_play(knocker_file_news, 3);
    } else {
      knocker_play(knocker_file_out, 0);
    }
    return true;
  case slai_talk_op::knocker_pose: // 0x2706A0
    knocker_play(arg(0), arg(1));
    return true;
  case slai_talk_op::portrait: // 0x26F510
    if (arg(0) == portrait_.character && arg(1) == portrait_.expression) {
      return false;
    }
    new_portrait(arg(0), arg(1));
    portrait_chip_ = arg(2) >= 0 ? arg(2) : chip_icon_of(arg(0));
    return true;
  default:
    return false;
  }
}

int SlaiTalkScene::chip_icon_of(int character) const {
  const auto *row = table_.character(character);
  return row != nullptr ? row->chip_icon : 0;
}

// 0x4EF1C0: the new portrait fades in over the old (the same character
// cross-fades; another swaps through its chip icon).
void SlaiTalkScene::new_portrait(int character, int expression) {
  portrait_same_ = character != 0 && character == portrait_.character;
  const bool had = portrait_.character != 0;
  previous_portrait_ = portrait_;
  portrait_ = {};
  portrait_.character = character;
  portrait_.expression = expression;
  if (const auto *row = table_.character(character); row != nullptr && !row->folder.empty()) {
    const int picture = character == 0x1F ? expression >> 2 : expression / 5;
    portrait_.texture = hex_name(row->folder, picture, ".png");
    const AnbBank *frames = bank(hex_name(row->folder, expression, ".json"));
    for (auto *object : {&portrait_.body, &portrait_.mouth, &portrait_.eyes}) {
      object->bank = frames;
      object->x = portrait_x;
      object->y = portrait_y;
    }
    portrait_.body.play(0);
    portrait_.mouth.anim = 1;
    portrait_.eyes.anim = 2;
  }
  if (portrait_same_) {
    set_alpha(portrait_.body, 0);
    return;
  }
  portrait_counter_ = had ? 0 : portrait_out_frames;
  objects_[slai_talk_stage::nameplate].flags |= anb_flag::hidden;
  clear_window_text(0);
  set_alpha(portrait_.body, 0);
}

bool SlaiTalkScene::poll_cast(int op) {
  switch (op) {
  case slai_talk_op::knocker_in:
    return poll_knocker_in();
  case slai_talk_op::knocker_out: // 0x2705A0
    if (!knocker_.has(anb_flag::ended)) {
      return false;
    }
    knocker_.flags |= anb_flag::hidden;
    return true;
  case slai_talk_op::knocker_pose:
    return knocker_.has(anb_flag::ended);
  case slai_talk_op::portrait:
    return poll_portrait();
  default:
    return true;
  }
}

// 0x2702E0
bool SlaiTalkScene::poll_knocker_in() {
  if (knocker_state_ == 1) {
    if (!knocker_.has(anb_flag::ended)) {
      return false;
    }
    knocker_play(knocker_file_news, 0);
    return true;
  }
  if (knocker_state_ != 2) {
    return knocker_.has(anb_flag::ended);
  }
  if (knocker_phase_ == 2) {
    return knocker_.has(anb_flag::ended);
  }
  if (knocker_phase_ == 1) {
    if (knocker_.has(anb_flag::ended)) {
      knocker_play(knocker_file_run, 0);
      ++knocker_phase_;
    }
    return false;
  }
  // Running in from the left, hopping over the last stretch.
  constexpr int hop_far = 0xD0, hop_near = 0xBE;
  constexpr int run_y = -0x7E, stand_y = -0x6C, raised = 40;
  const int x = knocker_x_ + knocker_run_step;
  const int left = knocker_target_x_ - x;
  int y = left > hop_far ? run_y : left > hop_near ? stand_y - (left - hop_near) : stand_y;
  if (town_hud_ == 2) {
    y += raised;
  }
  if (x < knocker_target_x_) {
    knocker_place(x, y);
    return false;
  }
  knocker_place(knocker_target_x_, y);
  if (knocker_.has(anb_flag::ended)) {
    knocker_play(knocker_file_run, 1);
    ++knocker_phase_;
  }
  return false;
}

// 0x26F600
bool SlaiTalkScene::poll_portrait() {
  if (portrait_same_) {
    const int alpha =
        std::min(0x80, static_cast<int>(portrait_.body.colour >> 24U) + crossfade_step);
    set_alpha(portrait_.body, alpha);
    set_alpha(previous_portrait_.body, 0x80 - alpha);
    if (alpha < 0x80) {
      return false;
    }
  } else {
    ++portrait_counter_;
    if (portrait_counter_ <= portrait_out_frames) {
      set_alpha(previous_portrait_.body,
                0x80 * (portrait_out_frames - portrait_counter_) / portrait_out_frames);
    }
    if (portrait_counter_ == portrait_out_frames && portrait_chip_ != 0 &&
        portrait_.character != 0) {
      icon_texture_ = character_texture(portrait_chip_);
      chip_.x = 256.0F;
      chip_.y = 224.0F;
      chip_.play(0x24);
    }
    if (portrait_counter_ > portrait_out_frames) {
      set_alpha(portrait_.body, 0x80 * (portrait_counter_ - portrait_out_frames) /
                                    (portrait_in_end - portrait_out_frames));
    }
    if (portrait_counter_ <= portrait_in_end) {
      return false;
    }
  }
  previous_portrait_ = {};
  blink_wait_ = 0;
  return true;
}

// 0x4ED590 and 0x4ED6F0, once a frame: the speaker's mouth and the blinks.
void SlaiTalkScene::animate_talk() {
  constexpr std::uint64_t portrait_bit = std::uint64_t{1} << slai_talk_op::portrait;
  constexpr std::uint64_t say_bit = std::uint64_t{1} << slai_talk_op::say;
  const bool talking = talk_sounds_ != 0 && (pending_ & portrait_bit) == 0 &&
                       (pending_ & say_bit) != 0 && windows_[0].open;
  if (talking && windows_[0].style == 2 && knocker_.bank != nullptr &&
      knocker_.has(anb_flag::ended)) {
    if (knocker_state_ == knocker_state_point) {
      knocker_play(knocker_file_point, 2);
    } else if (knocker_state_ == knocker_state_news) {
      knocker_play(knocker_file_news, 2);
    } else {
      knocker_play(knocker_file_talk, 0);
    }
  } else if (talking && windows_[0].style == 0 && portrait_.character >= 1 &&
             portrait_.character <= mouth_last_character && portrait_.mouth.has(anb_flag::hidden)) {
    portrait_.mouth.play(1, anb_flag::no_loop, 0x83);
  }
  if ((text_flags_ & 1U) == 0 && (pending_ & portrait_bit) == 0 && portrait_.character >= 1 &&
      portrait_.character <= eyes_last_character && --blink_wait_ < 1) {
    blink_wait_ = refresh_rate * 4 + static_cast<int>(next_random() % (refresh_rate * 2));
    portrait_.eyes.play(2, 0, 0x83);
  }
}

// The blink timer's rand(): a small LCG keeps scenes repeatable in tests.
unsigned SlaiTalkScene::next_random() {
  random_ = random_ * 1103515245U + 12345U;
  return (random_ >> 16U) & 0x7FFFU;
}

} // namespace phantomcrash
