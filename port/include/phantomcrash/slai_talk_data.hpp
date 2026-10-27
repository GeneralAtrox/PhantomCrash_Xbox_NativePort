#pragma once

// S.L.A.I.'s talk scenes as registration writes them (slai/talk_register.py):
// tables/slai_talk.json (opcodes, characters, story events, colours) and the
// EVT scripts under talk/event/<language>/. See docs/SLAI_STORY_PLAN.md and
// docs/SLAI_CAMPAIGN_RESEARCH.md, "Talk scenes".

#include "phantomcrash/json.hpp"

#include <array>
#include <cstdint>
#include <map>
#include <string>
#include <string_view>
#include <vector>

namespace phantomcrash {

// The opcodes the scene player knows by number (the table's order; the
// names are researcher labels).
namespace slai_talk_op {
inline constexpr int end = 0x00, backdrop_in = 0x01, haven_open = 0x02, haven_open_2 = 0x03,
                     haven_open_3 = 0x04, haven_close = 0x05, rings_out = 0x06, rings_out_2 = 0x07,
                     login_screen = 0x08, panel_open = 0x09, panel_close = 0x0A, open_window = 0x0B,
                     close_window = 0x0C, news_open = 0x0D, news_close = 0x0E,
                     preload_backgrounds = 0x0F, background = 0x10, event_portrait = 0x11,
                     event_portrait_close = 0x12, town_hud = 0x13, fade_in = 0x14, fade_out = 0x15,
                     load_portrait = 0x16, portrait = 0x17, badge = 0x18, badge_remove = 0x19,
                     load_icon = 0x1A, event_image = 0x1B, event_image_open = 0x1C,
                     event_image_close = 0x1D, badge_focus = 0x1E, wait_frames = 0x1F,
                     wait_button = 0x20, say = 0x21, load_wait = 0x22, knocker_in = 0x23,
                     knocker_out = 0x24, knocker_pose = 0x25, load_wait_2 = 0x26, partner_in = 0x27,
                     partner_out = 0x28, knocker_only_pose = 0x29, ambient_loop = 0x2A,
                     glitch = 0x2B, panel_sprites = 0x2C, partner_path = 0x2D, sync = 0x2E,
                     sync_portrait = 0x2F, sync_loads = 0x30, select_window = 0x31,
                     unload_character = 0x32, unload_all = 0x33, window_style = 0x34, case_ = 0x35,
                     glyph_size = 0x37, ruby_size = 0x38, text_speed = 0x39, text_cursor = 0x3A,
                     text_offset = 0x3B, clear_text = 0x3C, end_case = 0x3D, text_flag_1 = 0x3E,
                     line_feed = 0x3F, allow_skip = 0x40, talk_sounds = 0x41, bgm = 0x42,
                     bgm_stop = 0x43, se = 0x44, se_stop = 0x45, nop = 0x46, unload_knocker = 0x47,
                     unload_knocker_2 = 0x48, partner_path_stop = 0x49;
inline constexpr int count = 0x4A;
} // namespace slai_talk_op

// Glyph units: 0x00..0x5E are ASCII 0x20..0x7E; the rest are controls.
namespace slai_talk_glyph {
inline constexpr std::uint16_t newline = 0xFFFE, player = 0xFFFA, partner = 0xFFFB, alias = 0xFFFC,
                               day = 0xFFF7, style_end = 0xFF9C, style_first = 0xFF91,
                               style_last = 0xFF9B;
inline constexpr std::uint16_t ascii_limit = 0x5F;
} // namespace slai_talk_glyph

struct SlaiTalkOpcode {
  std::string name;
  int argc{};
  bool polls{};
  std::uint64_t waits{}; // bit n: op n still running holds this one back
  int string_arg{-1};
};

struct SlaiTalkInstruction {
  int op{};
  std::vector<int> args;
};

struct SlaiTalkScript {
  std::vector<SlaiTalkInstruction> code;
  std::vector<std::vector<std::uint16_t>> strings;
};

struct SlaiTalkCharacter {
  std::string folder; // talk/<folder>/ for portraits (2..0x39)
  std::string icon;   // 2d/s_chip/<icon>.png (0x3A on)
  std::string name;
  int chip_icon{};
  int expressions{};
  int pictures{};
};

struct SlaiTalkEvent {
  int index{};
  std::string name, kind;
  std::vector<std::string> flags;
  std::string place, sub_place;
  int city{-1};
  int threshold{};
  std::string condition;
  int condition_arg{};
  int required_flag{}, blocking_flag{}, completion_flag{};
  [[nodiscard]] bool has_flag(std::string_view flag) const;
};

// One bar of the portrait panel's voice meter (0x4ECBF0): its level swings
// between low and high with the period (frames) from the phase (radians).
struct SlaiTalkMeterBar {
  float low{}, high{}, phase{};
  int period{1};
};

// partner_path's offsets, one a frame, from the partner's place: up, or
// right when horizontal (0x271A40).
struct SlaiTalkPath {
  bool horizontal{};
  std::vector<int> steps;
};

struct SlaiTalkTable {
  std::vector<SlaiTalkOpcode> opcodes;
  std::map<int, SlaiTalkCharacter> characters;
  std::vector<SlaiTalkEvent> events;
  std::vector<std::string> news_pictures;
  std::vector<std::uint32_t> style_colours;
  std::vector<std::uint32_t> window_colours, prompt_colours;
  std::vector<std::array<std::uint32_t, 2>> style_text_colours; // text, name
  std::vector<std::array<int, 2>> panel_sprites;
  // ambient_loop's sound cues and sprite masks by loop (0 silence, none).
  std::vector<int> ambient_sounds, ambient_objects;
  // badge's joined-character places: x right of the centre, y up.
  std::vector<std::array<int, 2>> badge_slots;
  std::string badge_unknown_name;                 // a badge's name until the character is met
  std::vector<std::array<int, 2>> login_progress; // (frame, percent)
  std::string login_message, login_label;
  std::vector<std::string> knocker_textures;
  std::vector<std::vector<SlaiTalkMeterBar>> voice_meter; // idle, then by VM[0x1AD]
  int tutorial_record{};
  std::vector<SlaiTalkPath> partner_paths;

  [[nodiscard]] int opcode(std::string_view name) const; // -1 when unknown
  [[nodiscard]] const SlaiTalkCharacter *character(int id) const;
  [[nodiscard]] const SlaiTalkEvent *event(std::string_view name) const;
};

[[nodiscard]] SlaiTalkTable slai_talk_table_from_json(const json::JsonValue &value);
// Throws on unknown opcodes or wrong argument counts.
[[nodiscard]] SlaiTalkScript slai_talk_script_from_json(const json::JsonValue &value,
                                                        const SlaiTalkTable &table);

// Script text ("{style:2}FIRA{/style}", "{01a9}", "{player}") as glyph units.
[[nodiscard]] std::vector<std::uint16_t> slai_talk_glyphs(std::string_view text);

// font/glyphs.json: glyph g sits on page g / per_page, cell (g % per_page)
// in a grid of `columns`, advancing by width (left is its ink's offset).
struct SlaiTalkFont {
  std::vector<std::string> pages; // font/<page>
  int per_page{896};
  int columns{32};
  int cell_width{16}, cell_height{18};
  std::vector<int> left, width;
  [[nodiscard]] int advance(std::uint16_t glyph) const;
};

[[nodiscard]] SlaiTalkFont slai_talk_font_from_json(const json::JsonValue &value);

// GS colours are written "8080374b": alpha, blue, green, red.
[[nodiscard]] std::uint32_t parse_gs_colour(std::string_view text);

} // namespace phantomcrash
