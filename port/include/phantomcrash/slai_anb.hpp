#pragma once

// S.L.A.I.'s ANB 2D animation banks (talk/**/*.json, written by
// slai/anb.py) and the sprite engine's animated object that plays them
// (0x1CD4A0, stepped by 0x1CD8C0, drawn by 0x1CDD30). Positions are the
// PS2 screen's 512x448 pixels, y down; see docs/SLAI_CAMPAIGN_RESEARCH.md,
// "Talk scene assets".

#include "phantomcrash/json.hpp"

#include <array>
#include <cstdint>
#include <vector>

namespace phantomcrash {

enum class AnbBlend { opaque, alpha, additive };

struct AnbCell {
  int texture{};
  float u0{}, v0{}, u1{}, v1{};
  float x{}, y{}; // y up
  float half_width{}, half_height{};
  float scale_x{1.0F}, scale_y{1.0F};
  float rotation{}; // degrees
  float origin_x{}, origin_y{};
  float scale2_x{1.0F}, scale2_y{1.0F};
  float rotation2{};
  AnbBlend blend{AnbBlend::alpha};
  std::array<float, 4> rgba{255.0F, 255.0F, 255.0F, 255.0F}; // 0..255
};

enum class AnbOp { frame, tween, loop, jump, end };

struct AnbCommand {
  AnbOp op{AnbOp::end};
  float a{}; // frame: the frame shown; tween: steps; jump: the anim
  float b{}; // frame: ticks shown; tween: ticks per step
  float c{}; // tween: the easing (1 ease out, 2 ease in)
};

struct AnbBank {
  int textures{};
  std::vector<AnbCell> cells;
  std::vector<std::vector<int>> frames; // cell indices, drawn last to first
  std::vector<std::vector<AnbCommand>> anims;
};

[[nodiscard]] AnbBank anb_from_json(const json::JsonValue &value);

// The object's flag word (+0x34).
namespace anb_flag {
inline constexpr unsigned hidden = 0x1;  // neither stepped nor drawn
inline constexpr unsigned no_loop = 0x2; // a loop command ends the anim
inline constexpr unsigned hold = 0x4;    // the end holds the last frame
inline constexpr unsigned paused = 0x8;
inline constexpr unsigned ended = 0x80; // the anim has reached its end
} // namespace anb_flag

// The GS colour 0x80 means 1.0.
inline constexpr std::uint32_t gs_white = 0x80808080U;

struct AnbObject {
  const AnbBank *bank{};
  float x{256.0F}, y{224.0F};
  float scale_x{1.0F}, scale_y{1.0F};
  std::uint32_t colour{gs_white}; // GS RGBA, R in the low byte
  int z{};
  int anim{};
  int command{};
  float timer{};
  bool tween{};
  int tween_target{}; // the frame a tween ends on
  float tween_ticks{};
  float tween_ease{};
  unsigned flags{anb_flag::hidden};
  float speed{1.0F};

  // 0x1CD840: back to the first command; `add` keeps the time left over.
  void reset(bool add);
  // 0x1CD810: a different anim starts over.
  void set_anim(int value);
  // reset, set_anim, then set and clear flags, as the talk ops start anims.
  void play(int value, unsigned set = anb_flag::hold,
            unsigned clear = anb_flag::hidden | anb_flag::no_loop | anb_flag::ended);
  // 0x1CD8C0: `ticks` of 1/60 s.
  void step(float ticks);
  [[nodiscard]] bool has(unsigned flag) const {
    return (flags & flag) != 0;
  }
  [[nodiscard]] bool visible() const {
    return bank != nullptr && !has(anb_flag::hidden);
  }
};

struct AnbVertex {
  float x{}, y{};              // screen, y down
  float u{}, v{};              // texels
  std::array<float, 4> rgba{}; // 0..1
};

struct AnbQuad {
  int texture{};
  AnbBlend blend{AnbBlend::alpha};
  std::array<AnbVertex, 4> corners{}; // top left, top right, bottom left, bottom right
};

// The quads an object draws now, in draw order (0x1CDBE0, 0x1CDD30).
[[nodiscard]] std::vector<AnbQuad> anb_quads(const AnbObject &object);

// A GS colour as 0..1 floats (0x80 = 1).
[[nodiscard]] std::array<float, 4> gs_rgba(std::uint32_t colour);

} // namespace phantomcrash
