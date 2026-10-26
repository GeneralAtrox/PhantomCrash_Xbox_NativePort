// S.L.A.I. talk scenes' joined cast: badges on the HAVEN petals, the chip
// and character windows, the partner standing in as the Knocker, and the
// ambient loops.

#include "test_support.hpp"

#include "phantomcrash/slai_talk_scene.hpp"

#include <algorithm>
#include <map>
#include <string>

using namespace phantomcrash;

namespace {

const std::string lynx_face = "2d/s_chara/sab/sab012.png";
const std::string david_face = "2d/s_chara/sab/sab013.png";
const std::string knocker_picture = "talk/de000/de0013.png";

// Every anim, up to the windows' 0x25, shows the talk icon (texture 4) for
// two ticks and ends.
AnbBank short_bank() {
  std::string anims;
  for (int i = 0; i < 0x26; ++i) {
    anims += (i == 0 ? "" : ",");
    anims += R"([{"op": "frame", "a": 0, "b": 2}, {"op": "end"}])";
  }
  return anb_from_json(json::parse(R"({"textures": 5,
    "cells": [{"texture": 4, "u1": 16, "v1": 16, "x": 0, "y": 0, "half_width": 8,
               "half_height": 8, "rgba": [255, 255, 255, 255]}],
    "frames": [[0]], "anims": [)" + anims +
                                   "]}"));
}

SlaiTalkTable join_table() {
  using namespace slai_talk_op;
  SlaiTalkTable table;
  table.opcodes.resize(count);
  const std::map<int, std::pair<std::string, int>> used{
      {end, {"end", 0}},
      {wait_frames, {"wait_frames", 1}},
      {portrait, {"portrait", 3}},
      {badge, {"badge", 3}},
      {badge_remove, {"badge_remove", 1}},
      {badge_focus, {"badge_focus", 1}},
      {event_portrait, {"event_portrait", 1}},
      {event_image_open, {"event_image_open", 0}},
      {event_image_close, {"event_image_close", 0}},
      {partner_in, {"partner_in", 1}},
      {partner_out, {"partner_out", 1}},
      {ambient_loop, {"ambient_loop", 1}}};
  for (const auto &[op, row] : used) {
    table.opcodes[static_cast<std::size_t>(op)].name = row.first;
    table.opcodes[static_cast<std::size_t>(op)].argc = row.second;
  }
  // As shipped: the cast's entrances and exits hold the script up.
  std::uint64_t standard = 0;
  for (const int op : {wait_frames, wait_button, say, knocker_in, knocker_out, partner_in,
                       partner_out, portrait}) {
    standard |= std::uint64_t{1} << op;
  }
  for (auto &op : table.opcodes) {
    op.waits = standard;
  }
  table.opcodes[end].waits = ~0ULL;
  table.characters[5] = {"ab012", "", "Lynx", 6, 1, 1};
  table.characters[6] = {"ab013", "", "David", 0, 1, 1};
  table.badge_slots = {{41, 191}, {122, 159}, {177, 76}, {187, -11}};
  table.badge_unknown_name = "???";
  table.ambient_sounds = {0, 11, 12, 26};
  table.ambient_objects = {0, 1, 3, 4};
  table.knocker_textures.resize(0x14);
  table.knocker_textures[0x13] = knocker_picture;
  return table;
}

struct Stage {
  SlaiTalkTable table = join_table();
  SlaiTalkFont font;
  AnbBank bank = short_bank();
  SlaiTalkScene scene;
  explicit Stage(std::string_view code)
      : scene(table, font, slai_talk_script_from_json(json::parse(code), table), SlaiTalkMode::talk,
              [this](const std::string &) { return &bank; }) {}
  void run(int frames) {
    for (int i = 0; i < frames; ++i) {
      scene.tick({});
    }
  }
  [[nodiscard]] std::vector<SlaiTalkQuad> quads(const std::string &texture) const {
    std::vector<SlaiTalkQuad> out;
    std::ranges::copy_if(scene.draw(), std::back_inserter(out),
                         [&texture](const SlaiTalkQuad &quad) { return quad.texture == texture; });
    return out;
  }
};

} // namespace

TEST_CASE("A joined character's badge fades in on its petal, lights up and leaves") {
  Stage stage(R"({"code": [["badge", 5, 0, 0], ["wait_frames", 20], ["badge_focus", 5],
      ["wait_frames", 20], ["badge_remove", 5], ["wait_frames", 20], ["end"]], "strings": []})");
  stage.run(10);
  auto face = stage.quads(lynx_face);
  REQUIRE(face.size() == 1);
  // Centred 0xEA right of the slot and 0x10A down from its height.
  CHECK(face[0].corners[0].x == 41 + 0xEA - 28);
  CHECK(face[0].corners[0].y == 0x10A - 191 - 32);
  CHECK(face[0].corners[0].u == 4.0F);
  CHECK(face[0].corners[0].rgba[3] == 1.0F);
  CHECK(face[0].corners[0].rgba[0] == 0.5F); // dim until focused
  const auto glow = [&stage] {
    return std::ranges::count_if(stage.scene.draw(), [](const SlaiTalkQuad &quad) {
      return quad.blend == AnbBlend::additive && quad.texture == "talk/common/tal02.png";
    });
  };
  CHECK(glow() == 0);
  stage.run(20);
  face = stage.quads(lynx_face);
  REQUIRE(face.size() == 1);
  CHECK(face[0].corners[0].rgba[0] == 1.0F);
  CHECK(glow() == 1);
  stage.run(20);
  CHECK(stage.quads(lynx_face).empty());
  stage.run(15);
  CHECK(stage.scene.done());
}

TEST_CASE("The chip window opens with its sound, stays as the character window and closes") {
  Stage stage(R"({"code": [["event_portrait", 5], ["wait_frames", 10], ["event_image_close"],
      ["wait_frames", 20], ["event_image_open"], ["wait_frames", 10], ["end"]], "strings": []})");
  stage.run(1);
  CHECK(stage.scene.take_sounds() == std::vector<std::string>{"slai/assets/sound/cue_042.wav"});
  stage.run(2);
  CHECK(stage.quads(lynx_face).size() == 2); // the window and its shade
  stage.run(20);
  CHECK(stage.quads(lynx_face).size() == 1); // the shade has faded
  stage.run(20);
  CHECK(stage.quads(lynx_face).empty());
  CHECK(stage.scene.done());
}

TEST_CASE("A scene's first portrait opens its chip's character window") {
  Stage stage(R"({"code": [["portrait", 5, 0, 6], ["wait_frames", 30], ["end"]],
      "strings": []})");
  stage.run(3);
  CHECK(stage.quads(david_face).size() == 1);
}

TEST_CASE("Without a partner chip the partner ops bring the Knocker in and out") {
  Stage stage(R"({"code": [["partner_in", 0], ["partner_out", 0], ["end"]], "strings": []})");
  stage.run(1);
  CHECK(stage.quads(knocker_picture).size() == 1);
  CHECK(!stage.scene.done());
  stage.run(10);
  CHECK(stage.scene.done());
}

TEST_CASE("Ambient loops fade in, cross-fade and stop") {
  Stage stage(R"({"code": [["ambient_loop", 1], ["wait_frames", 40], ["ambient_loop", 3],
      ["wait_frames", 40], ["ambient_loop", 0], ["wait_frames", 40], ["end"]], "strings": []})");
  stage.run(1);
  REQUIRE(stage.scene.loops().size() == 1);
  CHECK(stage.scene.loops()[0].sound == "slai/assets/sound/cue_011.wav");
  CHECK(stage.scene.loops()[0].gain() < 0.1F);
  stage.run(35);
  CHECK(stage.scene.loops()[0].gain() == 1.0F);
  stage.run(10); // the second loop has started; the first fades out
  REQUIRE(stage.scene.loops().size() == 2);
  CHECK(stage.scene.loops()[1].sound == "slai/assets/sound/cue_026.wav");
  stage.run(30);
  REQUIRE(stage.scene.loops().size() == 1);
  CHECK(stage.scene.loops()[0].sound == "slai/assets/sound/cue_026.wav");
  stage.run(40);
  CHECK(stage.scene.loops().empty());
}
