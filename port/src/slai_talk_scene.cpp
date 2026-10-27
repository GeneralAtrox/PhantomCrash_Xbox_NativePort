#include "phantomcrash/slai_talk_scene.hpp"

#include <algorithm>
#include <bit>
#include <cstdio>
#include <utility>

namespace phantomcrash {
namespace {

// One frame runs at most this many instructions, so a script of
// non-waiting ops cannot hang the game.
constexpr int max_ops_per_frame = 4096;
constexpr std::uint64_t all_ops = ~std::uint64_t{0};

// The ops slai_talk_join.cpp runs.
bool join_op(int op) {
  switch (op) {
  case slai_talk_op::event_portrait:
  case slai_talk_op::event_portrait_close:
  case slai_talk_op::badge:
  case slai_talk_op::badge_remove:
  case slai_talk_op::event_image:
  case slai_talk_op::event_image_open:
  case slai_talk_op::event_image_close:
  case slai_talk_op::badge_focus:
  case slai_talk_op::partner_in:
  case slai_talk_op::partner_out:
  case slai_talk_op::knocker_only_pose:
  case slai_talk_op::ambient_loop:
  case slai_talk_op::partner_path:
  case slai_talk_op::unload_character:
  case slai_talk_op::unload_all:
  case slai_talk_op::unload_knocker:
  case slai_talk_op::unload_knocker_2:
  case slai_talk_op::partner_path_stop:
    return true;
  default:
    return false;
  }
}

std::string numbered(const char *format, int value) {
  std::array<char, 64> buffer{};
  std::snprintf(buffer.data(), buffer.size(), format, value);
  return buffer.data();
}

} // namespace

SlaiTalkScene::SlaiTalkScene(const SlaiTalkTable &table, const SlaiTalkFont &font,
                             SlaiTalkScript script, SlaiTalkMode mode, SlaiTalkBankLoader banks,
                             SlaiTalkSceneOptions options)
    : table_(table), font_(font), script_(std::move(script)), mode_(mode), banks_(std::move(banks)),
      names_(std::move(options.names)), selector_(options.partner.selector),
      partner_texture_(std::move(options.partner.texture)) {
  icons_.fill(-1);
  text_rate(options.text_rate);
  // A talk scene opens on black (the scene before faded out); a town
  // scene opens over the town.
  fade_ = mode_ == SlaiTalkMode::talk ? 1.0F : 0.0F;
  make_stage();
  if (options.glitched) {
    glitch_hidden_.set();
  }
}

// 0x20A920: the text rate; typing advances speed / (60 / rate) glyphs a frame.
void SlaiTalkScene::text_rate(int rate) {
  interval_ = std::max(1, refresh_rate / std::max(1, rate));
}

const AnbBank *SlaiTalkScene::bank(const std::string &path) const {
  return banks_ ? banks_(path) : nullptr;
}

void SlaiTalkScene::sound(int cue) {
  if (cue >= 0) {
    sounds_.push_back(numbered("slai/assets/sound/cue_%03d.wav", cue));
  }
}

std::vector<std::string> SlaiTalkScene::take_sounds() {
  return std::exchange(sounds_, {});
}

// 0x4EEA20: a character's small icon. Character 1 is the player's own
// (save+0x4438); the port shows the partner chip's until that is decoded.
std::string SlaiTalkScene::character_texture(int character) const {
  const auto *row = table_.character(character);
  if (row == nullptr) {
    return {};
  }
  if (!row->icon.empty()) {
    return "2d/s_chip/" + row->icon + ".png";
  }
  if (row->folder.size() < 3) {
    return {};
  }
  return "2d/s_chara/s" + row->folder.substr(0, 2) + "/s" + row->folder + ".png";
}

void SlaiTalkScene::step_objects(float ticks) {
  for (auto &object : objects_) {
    object.step(ticks);
  }
  for (auto &window : windows_) {
    window.frame.step(ticks);
    window.layer.step(ticks);
  }
  knocker_.step(ticks);
  for (auto *portrait : {&portrait_, &previous_portrait_}) {
    portrait->body.step(ticks);
    portrait->mouth.step(ticks);
    portrait->eyes.step(ticks);
  }
  chip_.step(ticks);
  chip_window_.step(ticks);
}

void SlaiTalkScene::tick(const SlaiTalkInput &input) {
  if (done_) {
    return;
  }
  ++frame_;
  input_.advance = input_.advance || input.advance;
  input_.hurry = input.hurry;
  if (input.skip_all) {
    done_ = true;
    music_.clear();
    loops_.clear();
    return;
  }
  step_objects(1.0F);
  step_loops();
  if (fade_step_ != 0.0F) {
    fade_ = std::clamp(fade_ + fade_step_, 0.0F, 1.0F);
    if (fade_ == 0.0F || fade_ == 1.0F) {
      fade_step_ = 0.0F;
    }
  }
  if (blip_wait_ > 0) {
    --blip_wait_;
  }
  step_meter();
  run_vm();
  input_.advance = false;
}

void SlaiTalkScene::run_vm() {
  // Running ops first, each poll once a frame (0x4EB560).
  for (std::uint64_t left = pending_; left != 0; left &= left - 1) {
    const int op = std::countr_zero(left);
    if (poll(op)) {
      pending_ &= ~(std::uint64_t{1} << op);
    }
  }
  animate_talk();
  for (int guard = 0; guard < max_ops_per_frame; ++guard) {
    if (pc_ >= static_cast<int>(script_.code.size())) {
      done_ = pending_ == 0;
      return;
    }
    const auto &instruction = script_.code[static_cast<std::size_t>(pc_)];
    const bool branch_op =
        instruction.op == slai_talk_op::case_ || instruction.op == slai_talk_op::end_case;
    if (case_value_ >= 0 && case_value_ != selector_ && !branch_op) {
      ++pc_; // another case's op
      continue;
    }
    const auto &row = table_.opcodes.at(static_cast<std::size_t>(instruction.op));
    if ((pending_ & row.waits) != 0) {
      return;
    }
    ++pc_;
    if (instruction.op == slai_talk_op::end) {
      done_ = true;
      return;
    }
    if (execute(instruction) && instruction.op < 64) {
      pending_ |= std::uint64_t{1} << instruction.op;
    }
  }
}

bool SlaiTalkScene::execute(const SlaiTalkInstruction &instruction) {
  const auto &args = instruction.args;
  const auto arg = [&](std::size_t i) { return i < args.size() ? args[i] : 0; };
  switch (instruction.op) {
  case slai_talk_op::wait_frames:
    wait_ = arg(0);
    return wait_ > 0;
  case slai_talk_op::sync:
  case slai_talk_op::sync_portrait:
  case slai_talk_op::sync_loads:
  case slai_talk_op::nop:
    return false; // their wait masks do the waiting
  case slai_talk_op::end_case:
    case_value_ = -1;
    return false;
  // Loads finish at once here: the port reads files as they are drawn.
  case slai_talk_op::preload_backgrounds:
  case slai_talk_op::load_portrait:
  case slai_talk_op::load_wait:
  case slai_talk_op::load_wait_2:
    return false;
  case slai_talk_op::load_icon:
    for (auto &slot : icons_) {
      if (slot == arg(0)) {
        return false;
      }
    }
    for (auto &slot : icons_) {
      if (slot < 0) {
        slot = arg(0);
        break;
      }
    }
    return false;
  case slai_talk_op::text_flag_1: // 0x270B70: 0 sets VM[6] bit 1
    text_flags_ = arg(0) != 0 ? text_flags_ & ~1U : text_flags_ | 1U;
    return false;
  case slai_talk_op::allow_skip: // 0x270BC0: 0 stops presses skipping the typing
    text_flags_ = arg(0) != 0 ? text_flags_ & ~2U : text_flags_ | 2U;
    return false;
  case slai_talk_op::line_feed: // 0x270C10: 0 keeps the next say on this line
    text_flags_ = arg(0) != 0 ? text_flags_ & ~4U : text_flags_ | 4U;
    return false;
  case slai_talk_op::talk_sounds: // 0x270B40: VM[0x1AD], the blip and talk animation
    talk_sounds_ = arg(0);
    return false;
  case slai_talk_op::bgm: // 0x270C60 -> 0x21BAB0: the id clamped to 0..0x76 is the stream
    music_ = numbered("sound/slai/bgm_%02d.ogg", std::clamp(arg(0), 0, 0x76));
    return false;
  case slai_talk_op::bgm_stop:
    music_.clear();
    return false;
  case slai_talk_op::se:
    sound(arg(0));
    return false;
  case slai_talk_op::se_stop:
    return false;
  default:
    break;
  }
  if (join_op(instruction.op)) {
    return exec_join(instruction.op, args);
  }
  if (instruction.op <= slai_talk_op::background || instruction.op == slai_talk_op::town_hud ||
      instruction.op == slai_talk_op::fade_in || instruction.op == slai_talk_op::fade_out ||
      instruction.op == slai_talk_op::glitch || instruction.op == slai_talk_op::panel_sprites) {
    if (instruction.op != slai_talk_op::open_window &&
        instruction.op != slai_talk_op::close_window) {
      return exec_stage(instruction.op, args);
    }
  }
  if (instruction.op == slai_talk_op::portrait || instruction.op == slai_talk_op::knocker_in ||
      instruction.op == slai_talk_op::knocker_out || instruction.op == slai_talk_op::knocker_pose) {
    return exec_cast(instruction.op, args);
  }
  return exec_text(instruction.op, args);
}

bool SlaiTalkScene::poll(int op) {
  if (join_op(op)) {
    return poll_join(op);
  }
  switch (op) {
  case slai_talk_op::wait_frames:
    return --wait_ <= 0;
  case slai_talk_op::portrait:
  case slai_talk_op::knocker_in:
  case slai_talk_op::knocker_out:
  case slai_talk_op::knocker_pose:
    return poll_cast(op);
  case slai_talk_op::open_window:
  case slai_talk_op::close_window:
  case slai_talk_op::wait_button:
  case slai_talk_op::say:
    return poll_text(op);
  default:
    return poll_stage(op);
  }
}

} // namespace phantomcrash
