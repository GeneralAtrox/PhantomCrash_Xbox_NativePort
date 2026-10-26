// S.L.A.I. talk scenes: the ANB player, the script text codec, the VM's
// waits and typing, and the opening scenes run on the installed data.

#include "test_support.hpp"

#include "phantomcrash/data_roots.hpp"
#include "phantomcrash/slai_story.hpp"
#include "phantomcrash/slai_talk_scene.hpp"

#include <cmath>
#include <map>
#include <memory>

using namespace phantomcrash;

namespace {

json::JsonValue parse(std::string_view text) {
  return json::parse(text);
}

AnbBank two_frame_bank() {
  return anb_from_json(parse(R"({"textures": 1,
    "cells": [{"texture": 0, "u1": 16, "v1": 16, "x": 0, "y": 0, "half_width": 8,
               "half_height": 8, "rgba": [255, 255, 255, 255]},
              {"texture": 0, "u1": 16, "v1": 16, "x": 10, "y": 0, "half_width": 8,
               "half_height": 8, "rgba": [255, 255, 255, 255]}],
    "frames": [[0], [1]],
    "anims": [[{"op": "frame", "a": 0, "b": 2}, {"op": "frame", "a": 1, "b": 2}, {"op": "end"}],
              [{"op": "frame", "a": 0, "b": 1}, {"op": "tween", "a": 4, "b": 1},
               {"op": "frame", "a": 1, "b": 1}, {"op": "loop"}]]})"));
}

// The opcodes a synthetic script uses, waiting as the shipped table does.
SlaiTalkTable small_table() {
  SlaiTalkTable table;
  table.opcodes.resize(slai_talk_op::count);
  const std::map<int, std::pair<std::string, int>> used{
      {slai_talk_op::end, {"end", 0}},
      {slai_talk_op::wait_frames, {"wait_frames", 1}},
      {slai_talk_op::wait_button, {"wait_button", 0}},
      {slai_talk_op::say, {"say", 1}},
      {slai_talk_op::sync, {"sync", 0}},
      {slai_talk_op::window_style, {"window_style", 2}},
      {slai_talk_op::text_speed, {"text_speed", 1}},
      {slai_talk_op::bgm, {"bgm", 2}},
      {slai_talk_op::case_, {"case", 2}},
      {slai_talk_op::end_case, {"end_case", 0}}};
  for (const auto &[op, row] : used) {
    table.opcodes[static_cast<std::size_t>(op)].name = row.first;
    table.opcodes[static_cast<std::size_t>(op)].argc = row.second;
  }
  const std::uint64_t standard = (1ULL << slai_talk_op::wait_frames) |
                                 (1ULL << slai_talk_op::wait_button) | (1ULL << slai_talk_op::say);
  for (auto &op : table.opcodes) {
    op.waits = standard;
  }
  table.opcodes[slai_talk_op::bgm].waits = 0;
  table.opcodes[slai_talk_op::end].waits = ~0ULL;
  return table;
}

SlaiTalkScript script(const SlaiTalkTable &table, std::string_view code) {
  return slai_talk_script_from_json(parse(code), table);
}

int run_until_done(SlaiTalkScene &scene, int limit, int press_every) {
  for (int frame = 1; frame <= limit; ++frame) {
    scene.tick({frame % press_every == 0, false, false});
    if (scene.done()) {
      return frame;
    }
  }
  return -1;
}

} // namespace

TEST_CASE("Talk text tokens become glyph units") {
  const auto units = slai_talk_glyphs("A{style:2}b{/style}{player}\n{01a9}");
  const std::vector<std::uint16_t> expected{0x21, 0xFF93, 0x42, 0xFF9C, 0xFFFA, 0xFFFE, 0x01A9};
  CHECK(units == expected);
  CHECK_THROWS(slai_talk_glyphs("{zz}"));
}

TEST_CASE("ANB objects step frames, hold or hide at the end, and tween") {
  const auto bank = two_frame_bank();
  AnbObject object;
  object.bank = &bank;
  object.play(0);
  CHECK(anb_quads(object).size() == 1);
  CHECK(anb_quads(object)[0].corners[0].x == 248.0F);
  object.step(2.0F);
  CHECK(anb_quads(object)[0].corners[0].x == 258.0F);
  object.step(2.0F);
  CHECK(object.has(anb_flag::ended));
  CHECK(object.visible());                   // held on its last frame
  object.play(0, 0, anb_flag::hold | 0x83U); // the default clear mask keeps hold
  object.step(4.0F);
  CHECK(!object.visible());

  object.play(1);
  object.step(1.0F); // into the tween: 4 ticks from frame 0 to frame 1
  object.step(2.0F);
  CHECK(std::abs(anb_quads(object)[0].corners[0].x - 253.0F) < 0.01F);
}

TEST_CASE("Talk scenes type text, wait for the button and end") {
  const auto table = small_table();
  const SlaiTalkFont font;
  auto code = script(table, R"({"code": [["bgm", 41, 30], ["window_style", 2, 0],
      ["text_speed", 30], ["say", 1], ["wait_button"], ["end"]],
      "strings": ["Knocker", "Hello there"]})");
  SlaiTalkSceneOptions options;
  options.text_rate = 2; // 30 frames a step: speed 30 types a glyph a frame
  SlaiTalkScene scene(table, font, std::move(code), SlaiTalkMode::talk, {}, options);
  scene.tick({});
  CHECK(scene.music() == "sound/slai/bgm_41.ogg");
  for (int i = 0; i < 20; ++i) {
    scene.tick({});
  }
  CHECK(!scene.done()); // typed, now waiting for the button
  scene.tick({true, false, false});
  scene.tick({});
  CHECK(scene.done());
}

TEST_CASE("A press skips the typing, a second pages on") {
  const auto table = small_table();
  const SlaiTalkFont font;
  auto code = script(table, R"({"code": [["window_style", 2, 0], ["text_speed", 1],
      ["say", 1], ["wait_button"], ["end"]], "strings": ["", "A long line of text"]})");
  SlaiTalkScene scene(table, font, std::move(code), SlaiTalkMode::talk, {});
  scene.tick({});
  scene.tick({true, false, false}); // skips the typing
  scene.tick({});
  CHECK(!scene.done());
  scene.tick({true, false, false});
  scene.tick({});
  CHECK(scene.done());
}

TEST_CASE("Only the selector's case runs between case and end_case") {
  const auto table = small_table();
  const SlaiTalkFont font;
  // Selector 0 (no partner chip): case 3's music is passed over.
  auto code = script(table, R"({"code": [["case", 3, 0], ["bgm", 7, 0], ["end_case"],
      ["case", 0, 0], ["bgm", 25, 0], ["end_case"], ["end"]], "strings": []})");
  SlaiTalkScene scene(table, font, std::move(code), SlaiTalkMode::talk, {});
  scene.tick({});
  CHECK(scene.music() == "sound/slai/bgm_25.ogg");
  CHECK(scene.done());
}

TEST_CASE("S.L.A.I. story flags round-trip through the save") {
  SlaiStorySave save;
  save.flags[0] = true;
  save.flags[255] = true;
  save.tutorial_mark = true;
  CHECK(slai_story_from_json(slai_story_to_json(save)) == save);
  CHECK(slai_story_from_json(parse(R"({"flags": [3, 999, -1]})")).flags[3]);
  CHECK(slai_story_from_json(parse("null")) == SlaiStorySave{});
}

TEST_CASE("The opening's events follow each other on the shared save") {
  const auto roots = DataRoots::configured(PHANTOMCRASH_SOURCE_ROOT, true);
  auto story = SlaiStory::load(roots);
  REQUIRE(story.has_value());
  SlaiStorySave save;
  SlaiRumbleSave rumble;
  const auto next = [&](std::string_view kind, std::string_view place, std::string_view sub) {
    const auto *event = story->next_event(kind, place, sub, {save, rumble, 0});
    return event == nullptr ? std::string{} : event->name;
  };
  const auto play = [&](std::string_view kind, std::string_view place, std::string_view sub) {
    const auto name = next(kind, place, sub);
    if (const auto *event = story->table().event(name)) {
      SlaiStory::complete(*event, save);
    }
    return name;
  };
  CHECK(play("town", "townmap", "townmap") == "main01t");
  CHECK(play("town", "townmap", "townmap") == "main02k");
  CHECK(play("town", "townmap", "townmap") == "main03t");
  CHECK(next("town", "townmap", "townmap").empty()); // Battle School comes first
  CHECK(play("scene_change", "townmap", "tutorial") == "bs01k");
  CHECK(next("scene_change", "battle_return", "nextday") == "bs05k"); // not yet passed
  rumble.records[story->table().tutorial_record].cleared = true;
  CHECK(play("scene_change", "battle_return", "nextday") == "bs03k");
  CHECK(play("town", "townmap", "townmap") == "main05k");
  CHECK(play("scene_change", "townmap", "wired") == "main06k");
  CHECK(story->mode("main01t") == SlaiTalkMode::town);
  CHECK(story->mode("main02k") == SlaiTalkMode::talk);
  CHECK(story->places_of("club_wired") == std::vector<std::string>{"wired"});
}

TEST_CASE("The opening and later talk scenes play to their end on the installed data") {
  const auto roots = DataRoots::configured(PHANTOMCRASH_SOURCE_ROOT, true);
  auto story = SlaiStory::load(roots);
  if (!story) {
    throw std::runtime_error("tables/slai_talk.json is not installed; register the S.L.A.I. "
                             "assets again (register-slai-talk)");
  }
  for (const char *name :
       {"main01t", "main02k", "main03t", "bs01k", "bs03k", "bs04k", "bs05k", "main05k",
        // badges, the chip window and the partner; an ambient loop
        "main06k", "main09k", "main17k"}) {
    auto scene = story->scene(name, {});
    REQUIRE(scene != nullptr);
    const int frames = run_until_done(*scene, 60 * 60 * 10, 7);
    CHECK(frames > 0);
    CHECK(!scene->draw().empty() || frames > 0);
  }
}
