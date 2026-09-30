#pragma once

#include "phantomcrash/json.hpp"

#include <array>
#include <string>

namespace phantomcrash {

// A robot walking into the battle is shielded (robot +0x27d4, in frames):
// 300 frames from its entrance (0x082552), refreshed throughout the player's
// entry and while computers wait at their gates (0x089032, 0x0892d1).
// A leaving robot is protected for its exit scene
// (0x08224d). No hit hurts it meanwhile. The shield's glow (robot +0xac,
// 0x082e60) rises 0.015 a frame to 0.3 while more than 10 frames are left,
// and falls 0.03 a frame after them.
struct EntryShield {
  float seconds{5.0F};            // 300 frames
  float fade_from{10.0F / 60.0F}; // the glow falls once less is left
  float peak{0.3F};               // the glow's most
  float rise_per_second{0.9F};    // 0.015 a frame
  float fall_per_second{1.8F};    // 0.03 a frame
};

// How the shield is drawn (0x06f592): object slot 0x41b, mesh 30 of
// resource 197 (a sphere one unit across), at the robot's lower collision
// sphere's centre, scaled by 3.2 leg radii, turned by the robot's heading
// plus an angle that grows 0.02 a frame, in colour 0x64ffffff times the
// glow, with draw mode 9 (added to what is behind it). The original's
// barrier program (Media/Shaders/barrier.xvu) lays systemdata/muteki01.tga
// over it; the extraction gives the sphere texture coordinates instead.
struct EntryShieldLook {
  std::string model{"models/binlink/197_30.gltf"};
  float scale{3.2F};           // times the leg radius
  float spin_per_second{1.2F}; // 0.02 a frame
  std::array<float, 4> colour{1.0F, 1.0F, 1.0F, 100.0F / 255.0F};
};

// data/world/training.json "entry_shield"; missing values keep the
// original's.
[[nodiscard]] EntryShield parse_entry_shield(const json::JsonValue &value);
[[nodiscard]] EntryShieldLook parse_entry_shield_look(const json::JsonValue &value);

// The glow after `seconds` more, with `left` of the shield left.
[[nodiscard]] float entry_shield_glow(const EntryShield &shield, float glow, float left, bool alive,
                                      float seconds);

} // namespace phantomcrash
