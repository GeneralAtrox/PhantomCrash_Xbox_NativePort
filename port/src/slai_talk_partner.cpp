// The partner chip in a talk scene (docs/SLAI_STORY_PLAN.md, "The partner
// chip"). With a partner (VM+0x770 not 0) the partner ops show the ridden
// chip's picture in TAL01's chip frame (object 15) instead of calling the
// Knocker, and partner_path moves it along one of the paths at 0x42DED0.
// In town the original shows it in the S.L.A.I. HUD's chip window, which
// the port does not have; the talk scene's frame stands in.

#include "phantomcrash/slai_talk_scene.hpp"

namespace phantomcrash {
namespace {

constexpr int partner_in_anim = 0x29, partner_out_anim = 0x2A;
// The nameplate's glow, which follows the partner along a path.
constexpr int nameplate_glow_anim = 0x2B;
constexpr unsigned partner_clear = anb_flag::hidden | anb_flag::ended; // 0x81
constexpr float partner_x = 256.0F, partner_y = 224.0F;
// This path's passes are given in frames, a pass taking 7.
constexpr int frame_counted_path = 2;
constexpr int frames_a_pass = 7;

} // namespace

bool SlaiTalkScene::exec_partner(int op, const std::vector<int> &args) {
  const auto arg = [&](std::size_t i) { return i < args.size() ? args[i] : 0; };
  auto &partner = objects_[slai_talk_stage::partner];
  switch (op) {
  case slai_talk_op::partner_in: // 0x270790
    partner.play(partner_in_anim, anb_flag::hold, partner_clear);
    return true;
  case slai_talk_op::partner_out: // 0x2708D0
    partner.play(partner_out_anim, anb_flag::hold, partner_clear);
    objects_[slai_talk_stage::nameplate].flags |= anb_flag::hidden;
    return true;
  case slai_talk_op::partner_path: { // 0x271980
    partner_path_ = arg(0);
    if (partner_path_ < 0 || partner_path_ >= static_cast<int>(table_.partner_paths.size()) ||
        table_.partner_paths[static_cast<std::size_t>(partner_path_)].steps.empty()) {
      return false;
    }
    const int repeats = arg(1);
    partner_repeats_ = repeats == 0 ? -1
                       : partner_path_ == frame_counted_path
                           ? (repeats + frames_a_pass - 1) / frames_a_pass
                           : repeats;
    partner_step_ = 0;
    return true;
  }
  case slai_talk_op::partner_path_stop: // the pass under way is the last
    partner_repeats_ = 1;
    return false;
  default:
    return false;
  }
}

bool SlaiTalkScene::poll_partner(int op) {
  auto &partner = objects_[slai_talk_stage::partner];
  switch (op) {
  case slai_talk_op::partner_in: // 0x270850
    return partner.has(anb_flag::ended);
  case slai_talk_op::partner_out: // 0x2709A0
    if (!partner.has(anb_flag::ended)) {
      return false;
    }
    partner.flags |= anb_flag::hidden;
    return true;
  case slai_talk_op::partner_path:
    return step_partner_path();
  default:
    return true;
  }
}

// 0x271A40: a step a frame, the partner offset from its place (up, or right
// on a horizontal path) and the nameplate's glow with it; passes repeat
// until none are left.
bool SlaiTalkScene::step_partner_path() {
  const auto &path = table_.partner_paths[static_cast<std::size_t>(partner_path_)];
  const auto offset =
      static_cast<float>(path.steps[static_cast<std::size_t>(partner_step_) % path.steps.size()]);
  const float x = path.horizontal ? partner_x + offset : partner_x;
  const float y = path.horizontal ? partner_y : partner_y - offset;
  auto &partner = objects_[slai_talk_stage::partner];
  partner.x = x;
  partner.y = y;
  auto &plate = objects_[slai_talk_stage::nameplate];
  const bool glowing = plate.anim == nameplate_glow_anim;
  plate.x = glowing ? x : partner_x;
  plate.y = glowing ? y : partner_y;
  if (++partner_step_ >= static_cast<int>(path.steps.size())) {
    if (partner_repeats_ >= 0 && --partner_repeats_ < 1) {
      return true;
    }
    partner_step_ = 0;
  }
  return false;
}

} // namespace phantomcrash
