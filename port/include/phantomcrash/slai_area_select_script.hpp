#pragma once

// S.L.A.I.'s area select (a port screen, data/ui/slai_area_select.json):
// Rumble S.L.A.I. drawn and driven as RUMBLING's area select is, over all
// seven of S.L.A.I.'s cities. Each city has its plate (RUMBLING's, its name
// redrawn as text), the blueprint from its Wired sheet in the picture frame
// and its emblem and DAY/NIGHT label as the title; the sub-menu, its map
// (the pack's blueprint of the day's stage), the gates and INFORMATION are
// RUMBLING's. A city the rota keeps closed today shows CLOSED.

#include "phantomcrash/data_roots.hpp"
#include "phantomcrash/json.hpp"
#include "phantomcrash/rumbling_script.hpp"
#include "phantomcrash/slai_rumble_hub.hpp"

#include <array>
#include <map>
#include <string>
#include <vector>

namespace phantomcrash {

// The title a city shows where RUMBLING's area logo goes: its emblem fitted
// into emblem_box, its "<CITY> / DAY" (or NIGHT) label and its venue's
// name, each from the title's place and scaled from the Wired sheet.
struct SlaiAreaTitle {
  std::array<float, 4> emblem_box{};
  std::array<float, 2> label_at{};
  float label_scale{1.0F};
  std::array<float, 2> venue_at{};
  float venue_scale{1.0F};
  int layer{0x102};
};

struct SlaiAreaSelectScript {
  // RUMBLING's art, timings and sub-menu; its areas are the city plate's
  // template (the first) and EXIT (the last).
  RumblingScript rumbling;
  // Rumble S.L.A.I.'s: the cities' art and names, the refusals, the class
  // badges and the records it leaves out.
  SlaiRumbleHubScript hub;
  std::string maps; // the pack's blueprint maps, by stage
  // Each city's plate by the table's display order, then EXIT's.
  std::vector<std::array<float, 2>> plates;
  std::array<float, 2> exit_plate{};
  float plate_slide{};  // the plates come in from this far left
  std::string plate_id; // + the city's column: its plate's name in sprite_text.json
  // RUMBLING's picture frame, CLOSED, noise, CLEAR and EXIT's picture and
  // label are drawn this much smaller about the frame's corner, making room
  // for the second row of plates.
  float picture_scale{1.0F};
  std::array<float, 4> picture_box{}; // the blueprint is fitted in here
  Sprite picture_back;                // under it
  SlaiAreaTitle title;
  std::string help;        // an open city's; {city} is its name
  std::string closed_help; // a closed city's
};

// Throws std::runtime_error naming the member at fault. `hub` is the parsed
// Rumble S.L.A.I. document named by "hub".
[[nodiscard]] SlaiAreaSelectScript parse_slai_area_select(const json::JsonValue &document,
                                                          SlaiRumbleHubScript hub);
// The screen's document with its hub's; nothing when either is missing.
[[nodiscard]] std::optional<SlaiAreaSelectScript> load_slai_area_select(const DataRoots &roots,
                                                                        const std::string &file);

// The pack's blueprint maps (data/ui/slai_area_maps.json, written by
// draw-slai-maps), by stage name: each an area holding only its map frames,
// gates and robot map.
using SlaiAreaMaps = std::map<std::string, RumblingArea>;
[[nodiscard]] SlaiAreaMaps parse_slai_area_maps(const json::JsonValue &document);

} // namespace phantomcrash
