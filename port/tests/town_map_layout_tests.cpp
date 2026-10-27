// The widescreen town map's layout: every place's disc and label on land,
// clear of each other and of the bands, and its rider clear of the other
// places, with S.L.A.I.'s places and without.

#include "test_support.hpp"

#include <algorithm>
#include <array>
#include <cmath>
#include <string>
#include <vector>

#include "phantomcrash/data_roots.hpp"
#include "phantomcrash/town_map.hpp"

using namespace phantomcrash;

namespace {

constexpr float sample_step = 1.0F; // canvas units between the points checked

TownMapScript shipped(const bool slai) {
  return for_content(
      parse_town_map(DataRoots::shipped(PHANTOMCRASH_SOURCE_ROOT).document("ui/town_map.json")),
      slai);
}

bool overlap(const std::array<float, 4> &a, const std::array<float, 4> &b) {
  return a[0] < b[2] && b[0] < a[2] && a[1] < b[3] && b[1] < a[3];
}

#define LAYOUT_CHECK(condition, what)                                                              \
  do {                                                                                             \
    if (!(condition)) {                                                                            \
      ::phantomcrash::test::fail(__FILE__, __LINE__, std::string(what) + ": " #condition);         \
    }                                                                                              \
  } while (false)

const Sprite &map_layer(const TownMapScript &script) {
  const auto found = std::ranges::find(script.layers, script.water->map, &Sprite::id);
  LAYOUT_CHECK(found != script.layers.end(), "the water's map layer");
  return *found;
}

// The points of `box` on water; `disc` keeps to the ellipse within it.
int wet_points(const TownMapScript &script, const TownMarkerBoxes &boxes,
               const std::array<float, 4> &box, const bool disc) {
  const Sprite &map = map_layer(script);
  int wet = 0;
  for (float y = box[1]; y <= box[3]; y += sample_step) {
    for (float x = box[0]; x <= box[2]; x += sample_step) {
      const float dx = (x - boxes.centre[0]) / boxes.radii[0];
      const float dy = (y - boxes.centre[1]) / boxes.radii[1];
      if ((!disc || dx * dx + dy * dy <= 1.0F) && on_water(*script.water, map, {x, y})) {
        ++wet;
      }
    }
  }
  return wet;
}

void check_layout(const TownMapScript &script) {
  REQUIRE(script.water.has_value());
  for (const TownPlace &place : script.places) {
    LAYOUT_CHECK(wet_points(script, place.boxes, place.boxes.disc, true) == 0, place.id);
    LAYOUT_CHECK(wet_points(script, place.boxes, place.boxes.label, false) == 0, place.id);
    for (const auto &clear : script.keep_clear) {
      const auto band = script.view.box(clear);
      LAYOUT_CHECK(!overlap(place.boxes.shadowed_disc, band), place.id);
      LAYOUT_CHECK(!overlap(place.boxes.label, band), place.id);
    }
  }
  // The rider at rest stands clear of the other places' markers and the bands.
  const float rider_width = script.rider.rect[2] - script.rider.rect[0];
  const float rider_height = script.rider.rect[3] - script.rider.rect[1];
  for (const TownPlace &place : script.places) {
    const std::array<float, 4> rider{place.rider[0], place.rider[1], place.rider[0] + rider_width,
                                     place.rider[1] + rider_height};
    for (const TownPlace &other : script.places) {
      if (&other != &place) {
        LAYOUT_CHECK(!overlap(rider, other.boxes.shadowed_disc), place.id + " / " + other.id);
        LAYOUT_CHECK(!overlap(rider, other.boxes.label), place.id + " / " + other.id);
      }
    }
    for (const auto &clear : script.keep_clear) {
      LAYOUT_CHECK(!overlap(rider, script.view.box(clear)), place.id + "'s rider");
    }
  }
  for (std::size_t a = 0; a < script.places.size(); ++a) {
    for (std::size_t b = a + 1; b < script.places.size(); ++b) {
      const TownMarkerBoxes &one = script.places[a].boxes;
      const TownMarkerBoxes &other = script.places[b].boxes;
      const std::string pair = script.places[a].id + " / " + script.places[b].id;
      LAYOUT_CHECK(!overlap(one.shadowed_disc, other.shadowed_disc), pair);
      LAYOUT_CHECK(!overlap(one.shadowed_disc, other.label), pair);
      LAYOUT_CHECK(!overlap(one.label, other.shadowed_disc), pair);
      LAYOUT_CHECK(!overlap(one.label, other.label), pair);
    }
  }
}

} // namespace

TEST_CASE("the town map's water is where the map shows it") {
  const TownMapScript script = shipped(false);
  REQUIRE(script.water.has_value());
  const Sprite &map = map_layer(script);
  CHECK(on_water(*script.water, map, script.view.point({1000.0F, 520.0F}))); // the bay
  CHECK(!on_water(*script.water, map, script.view.point({575.0F, 122.0F}))); // the garage
}

TEST_CASE("the town map's eight places stand on land, clear of each other and the bands") {
  check_layout(shipped(false));
}

TEST_CASE("with S.L.A.I. the town map's fifteen places stand on land, clear of each other") {
  check_layout(shipped(true));
}

TEST_CASE("the town map's markers and labels are where their spots put them") {
  const TownMapScript script = shipped(false);
  const TownPlace &garage = script.places[0];
  const auto centre = script.view.point({575.0F, 122.0F});
  CHECK(std::abs(garage.boxes.centre[0] - centre[0]) < 1e-3F);
  CHECK(std::abs(garage.boxes.centre[1] - centre[1]) < 1e-3F);
  // The garage's label is on the disc's left; Club Wired's on its right.
  CHECK(garage.boxes.label[2] < garage.boxes.disc[0]);
  const TownPlace &club = script.places[1];
  CHECK(club.boxes.label[0] > club.boxes.disc[2]);
}
