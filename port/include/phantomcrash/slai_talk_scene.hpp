#pragma once

// S.L.A.I.'s talk scene player: the EVT VM (talk.cpp / evtcmd.cpp /
// knocker.cpp in the shipped strings; dispatcher 0x4EB560) with its talk
// windows, the Knocker, portraits, the HAVEN backdrop, news pictures, fades
// and the lag glitch. Platform free: draw() lists quads in the PS2 screen's
// 512x448 pixels for the desktop to render. See docs/SLAI_STORY_PLAN.md.

#include "phantomcrash/slai_anb.hpp"
#include "phantomcrash/slai_talk_data.hpp"

#include <array>
#include <bitset>
#include <cstdint>
#include <functional>
#include <optional>
#include <string>
#include <vector>

namespace phantomcrash {

// Where a scene plays: '+' talk scenes (*k scripts) own the screen; town
// scenes (*t, the event table's town_overlay flag) draw over the town.
enum class SlaiTalkMode { talk, town };

struct SlaiTalkInput {
  bool advance{};  // a press that pages text and skips typing
  bool hurry{};    // the held pair that also skips typing
  bool skip_all{}; // the port's scene skip
};

struct SlaiTalkQuad {
  std::string texture; // asset path; empty for a solid colour
  AnbBlend blend{AnbBlend::alpha};
  std::array<AnbVertex, 4> corners{}; // top left, top right, bottom left, bottom right
};

struct SlaiTalkSceneNames {
  std::string player{"PLAYER"}, partner{"PARTNER"}, alias{"ALIAS"};
  int day{1};
};

// The partner chip (VM+0x770): which of a script's cases plays (0 the
// Knocker's, 1..8 a partner's) and the picture the partner ops show.
struct SlaiTalkPartner {
  int selector{};
  std::string texture;
};

// What a scene starts from (data/slai/talk_scenes.json): the game's text
// rate, which the opening's glitch leaves at 2 for the rest of the game,
// and whether the town's cells start hidden (MAIN03T opens by restoring
// what MAIN01T broke).
struct SlaiTalkSceneOptions {
  SlaiTalkSceneNames names;
  SlaiTalkPartner partner;
  int text_rate{2};
  bool glitched{};
};

// Loads what the scene names (talk/common/tal01.json, ...); null when missing.
using SlaiTalkBankLoader = std::function<const AnbBank *(const std::string &path)>;

struct SlaiTalkSprite {
  std::string texture;
  float u{}, v{}, w{}, h{};
  float x{256.0F}, y{224.0F};
  float scale_x{1.0F}, scale_y{1.0F};
  float rotation{}; // radians, clockwise
  float alpha{128.0F};
  std::uint32_t colour{gs_white};
  bool visible{};
  AnbBlend blend{AnbBlend::alpha}; // the sprite's +0x74 mode 1 adds
};

struct SlaiTalkText {
  std::vector<std::uint16_t> glyphs;
  float x{}, y{}, w{18.0F}, h{18.0F};
  std::uint32_t colour{gs_white};
  int shown{-1}; // glyphs typed so far; -1 all
};

struct SlaiTalkWindow {
  bool open{};
  AnbObject frame, layer;
  int style{};
  int character{};
  std::optional<SlaiTalkText> name;
  std::vector<SlaiTalkText> lines;
  std::uint32_t colour{gs_white};      // the window art's tint
  std::uint32_t text_colour{gs_white}; // the style's text colour
  float x{}, y{};                      // where the window sits
  float cursor_x{}, cursor_y{};        // where the next say starts
  float glyph_w{18.0F}, glyph_h{18.0F}, ruby_w{9.0F}, ruby_h{9.0F};
  int speed{1};
  int accumulator{};
};

struct SlaiTalkPortrait {
  int character{};
  int expression{-1};
  std::string texture;
  AnbObject body, mouth, eyes;
};

// A character who has joined, shown on a HAVEN petal (FaceWork,
// TALK_JOIN_CHARA_MAX; VM+0x490): its face and its name.
struct SlaiTalkBadge {
  int character{};
  SlaiTalkText name;
  SlaiTalkSprite face;
  bool shown{}; // VM+0x4C0's bit: coming in, not going out
};

// An ambient loop (ambient_loop): its sound and how far it has faded.
struct SlaiTalkLoop {
  std::string sound;
  int id{};   // tells loops of the same sound apart
  int fade{}; // frames of the fade done
  int step{}; // +1 fading in, -1 fading out
  static constexpr int fade_frames = 30;
  [[nodiscard]] float gain() const {
    return static_cast<float>(fade) / fade_frames;
  }
};

// SlaiTalkScene's object and sprite slots (VM+0x64.. and VM+0xC0..).
namespace slai_talk_stage {
inline constexpr int haven_a = 0, haven_b = 1, haven_c = 2, haven_d = 3, haven_e = 4, panel_a = 5,
                     panel_b = 6, panel_c = 7, panel_d = 8, chip_shade = 9, news_frame = 10,
                     news_anim = 11, panel_e = 12, prompt = 13, nameplate = 14, partner = 15,
                     ambient = 16, badge_frames = 19;
inline constexpr int objects = 0x17;
inline constexpr int ambient_count = 3, badge_count = 4;
inline constexpr int petals = 0, focus_glow = 12, lit_petal = 16, dim_petal = 20, ring_1 = 24,
                     ring_2 = 30, ring_3 = 38, meters = 44, sprite_count = 0x33;
} // namespace slai_talk_stage

// Sprites placed round the HAVEN ellipse: the first, how many, the degrees
// between them, the first angle and the radius.
struct SlaiTalkRing {
  int first, count;
  float step, start, radius;
};

class SlaiTalkScene {
public:
  SlaiTalkScene(const SlaiTalkTable &table, const SlaiTalkFont &font, SlaiTalkScript script,
                SlaiTalkMode mode, SlaiTalkBankLoader banks, SlaiTalkSceneOptions options = {});

  // One 1/60 s frame.
  void tick(const SlaiTalkInput &input);
  [[nodiscard]] std::vector<SlaiTalkQuad> draw() const;
  [[nodiscard]] SlaiTalkMode mode() const {
    return mode_;
  }
  [[nodiscard]] bool done() const {
    return done_;
  }
  // "" for silence, otherwise sound/slai/bgm_NN.ogg.
  [[nodiscard]] const std::string &music() const {
    return music_;
  }
  [[nodiscard]] std::vector<std::string> take_sounds();
  // The ambient loops playing or fading out.
  [[nodiscard]] const std::vector<SlaiTalkLoop> &loops() const {
    return loops_;
  }
  [[nodiscard]] int frame() const {
    return frame_;
  }
  [[nodiscard]] int pc() const {
    return pc_;
  }
  [[nodiscard]] int script_size() const {
    return static_cast<int>(script_.code.size());
  }
  [[nodiscard]] int text_interval() const {
    return interval_;
  }

  static constexpr int refresh_rate = 60;
  static constexpr int glitch_cells = 500;
  static constexpr int glitch_columns = 25;
  static constexpr int glitch_rows = 20;

private:
  // slai_talk_scene.cpp: the VM.
  void run_vm();
  bool execute(const SlaiTalkInstruction &instruction);
  bool poll(int op);
  void step_objects(float ticks);
  [[nodiscard]] const AnbBank *bank(const std::string &path) const;
  [[nodiscard]] std::string character_texture(int character) const;
  void sound(int cue);
  void text_rate(int rate);

  // slai_talk_stage.cpp: HAVEN, the backdrop, panel, news and pictures.
  void make_stage();
  void place_ring(const SlaiTalkRing &ring);
  void ring_alpha(const SlaiTalkRing &ring, int alpha);
  bool exec_stage(int op, const std::vector<int> &args);
  bool poll_stage(int op);
  bool exec_background(int next);
  bool poll_background();
  void step_meter();

  // slai_talk_effects.cpp: fades, the glitch, the login screen, panel
  // icons, the ambient loops.
  bool exec_effect(int op, const std::vector<int> &args);
  bool poll_effect(int op);
  bool poll_glitch();
  bool poll_login();
  bool exec_ambient(int loop);
  bool poll_ambient();
  void step_loops();

  // slai_talk_text.cpp: windows, styles, say, the prompt.
  bool exec_text(int op, const std::vector<int> &args);
  bool poll_text(int op);
  bool say(int string);
  void apply_style(int style, int name_string);
  [[nodiscard]] int look_ahead_style() const;
  void clear_window_text(int window);
  void reset_cursor(SlaiTalkWindow &window, int index) const;
  [[nodiscard]] std::vector<std::uint16_t> expand(const std::vector<std::uint16_t> &glyphs) const;
  [[nodiscard]] float text_width(const SlaiTalkText &text) const;
  [[nodiscard]] float glyph_advance(std::uint16_t glyph, float size) const;

  // slai_talk_cast.cpp: the Knocker, portraits, talking and blinking.
  bool exec_cast(int op, const std::vector<int> &args);
  bool poll_cast(int op);
  void knocker_play(int file, int anim);
  void knocker_place(int x, int y);
  bool poll_knocker_in();
  void new_portrait(int character, int expression);
  bool poll_portrait();
  [[nodiscard]] int chip_icon_of(int character) const;
  void animate_talk();
  unsigned next_random();

  // slai_talk_join.cpp: badges, the chip and character windows and the
  // partner.
  void make_badge_petals();
  bool exec_join(int op, const std::vector<int> &args);
  bool poll_join(int op);
  bool exec_badge(int character, int slot, bool unknown);
  bool poll_badge_in();
  bool exec_badge_remove(int character);
  bool poll_badge_out();
  void set_focus(int character);
  bool poll_focus();
  void open_character_window(int chip);
  void close_character_window();

  // slai_talk_partner.cpp: the partner chip's ops.
  bool exec_partner(int op, const std::vector<int> &args);
  bool poll_partner(int op);
  bool step_partner_path();

  // slai_talk_draw.cpp
  void text_quads(const SlaiTalkText &text, std::vector<SlaiTalkQuad> &out) const;
  void login_quads(std::vector<SlaiTalkQuad> &out) const;

  const SlaiTalkTable &table_;
  const SlaiTalkFont &font_;
  SlaiTalkScript script_;
  SlaiTalkMode mode_;
  SlaiTalkBankLoader banks_;
  SlaiTalkSceneNames names_;

  int pc_{};
  int frame_{};
  bool done_{};
  std::uint64_t pending_{};
  SlaiTalkInput input_{};
  // VM[6]: 1 no blinking, 2 typing cannot be skipped, 4 next say on the same
  // line, 0x40 typing skipped.
  unsigned text_flags_{};
  int selector_{};
  std::string partner_texture_;
  // partner_path (VM+0x764..0x76C): the path, the passes left (-1 for
  // ever) and the step.
  int partner_path_{}, partner_repeats_{}, partner_step_{};
  // VM + 0x1DD: the open case; while it differs from the selector, only
  // case and end_case ops run (-1: none open).
  int case_value_{-1};
  int talk_sounds_{1}; // VM[0x1AD]
  int blip_wait_{};
  int wait_{};
  int interval_{1};
  int town_hud_{};
  unsigned random_{1};

  std::array<AnbObject, slai_talk_stage::objects> objects_{};
  std::string icon_texture_; // TAL01's fifth texture (res+0x18)
  std::vector<SlaiTalkSprite> sprites_;
  SlaiTalkSprite backdrop_;
  SlaiTalkSprite picture_;
  int backdrop_steps_{};
  int counter_a_{}, counter_b_{}; // VM+0x24, VM+0x28
  int background_{}, background_next_{};
  std::array<float, 7> meter_levels_{};
  int meter_frame_{};

  std::array<SlaiTalkWindow, 4> windows_{};
  int window_{};
  int closing_{-1};

  AnbObject knocker_;
  std::string knocker_texture_;
  int knocker_file_{-1};
  int knocker_state_{};
  int knocker_phase_{};
  int knocker_x_{}, knocker_y_{};
  int knocker_target_x_{};

  SlaiTalkPortrait portrait_, previous_portrait_;
  AnbObject chip_;        // the character window (pCharaWindowChip, VM+0x69C)
  AnbObject chip_window_; // the chip window (pChipWindowChip, VM+0x6A0)
  int portrait_chip_{};
  int portrait_counter_{};
  bool portrait_same_{};
  int blink_wait_{};

  std::array<int, 16> icons_{}; // load_icon's slots (VM+0x3D0)
  std::array<SlaiTalkSprite, 7> panel_icons_{};
  int panel_fade_{};
  bool panel_fading_out_{};

  float fade_{};      // 0 clear .. 1 black
  float fade_step_{}; // per frame

  std::bitset<glitch_cells> glitch_hidden_;
  int glitch_counter_{};

  int login_frame_{-1};
  int login_percent_{};

  std::array<SlaiTalkBadge, slai_talk_stage::badge_count> badges_{};
  int focus_{}; // VM+0x4C1: the badge lit up

  int ambient_{}; // VM+0x6D8
  std::vector<SlaiTalkLoop> loops_;
  int next_loop_id_{1};

  std::string music_;
  std::vector<std::string> sounds_;
};

} // namespace phantomcrash
