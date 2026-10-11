// The CHIPs the port adds (data/tables/added_chips.json): records numbered on
// from the original's 96 and the BUY CHIP offers that list them.

#include "test_support.hpp"

#include <array>
#include <cmath>
#include <string>

#include "phantomcrash/added_chips.hpp"
#include "phantomcrash/data_roots.hpp"
#include "phantomcrash/json.hpp"
#include "phantomcrash/json_fields.hpp"
#include "phantomcrash/robot_parts.hpp"

using namespace phantomcrash;

namespace {

constexpr int original_chips = 96;
constexpr int shop_days = 24;

const char *const original_chip = R"({"index": 0, "name": "Dog I", "inner_shape": 0,
  "inner_value": {"base": 200, "mid": 560, "top": 700, "knee": 60, "low_scale": -1},
  "track": 3, "outer_shape": 2,
  "outer_value": {"base": 200, "mid": 595, "top": 800, "knee": 60, "low_scale": -1},
  "lock_frames": 22, "chatter": 0})";

json::JsonValue added() {
  return DataRoots::shipped(PHANTOMCRASH_SOURCE_ROOT).document(added_chips_document);
}

// A parts table holding `count` chips and no weapons.
json::JsonValue parts_with(const int count) {
  std::string text = R"({"chips": [)";
  for (int index = 0; index < count; ++index) {
    text += (index == 0 ? "" : ",") + std::string(original_chip);
  }
  return json::parse(text + R"(], "arm_weapons": [], "shoulder_weapons": []})");
}

bool near(const float a, const float b) {
  return std::fabs(a - b) < 1e-3F;
}

} // namespace

TEST_CASE("the added CHIPs follow the original's, one record per personality") {
  auto parts = parts_with(original_chips);
  CHECK(add_chips(parts, added()));
  const PartTables tables = parse_part_tables(parts);
  CHECK(tables.chips.size() == 111U);
  const std::array<const char *, 3> names{"Cat V", "Cat VI", "Cat VII"};
  for (int index = original_chips; index < 111; ++index) {
    const ChipRecord *chip = tables.chip(index);
    CHECK(chip != nullptr);
    if (chip == nullptr) {
      return;
    }
    const int offset = index - original_chips;
    CHECK(chip->index == index);
    CHECK(chip->name == names[static_cast<std::size_t>(offset / 5)]);
    CHECK(chip->chatter == offset % 5);
    CHECK(chip->face && chip->face->rect[2] == 64.0F && chip->face->rect[3] == 64.0F);
  }
  const ChipRecord &norwegian = *tables.chip(96);
  CHECK(norwegian.face->image == "textures/chips/cat_v.png");
  CHECK(norwegian.track == 3 && norwegian.lock_frames == 8);
  CHECK(near(norwegian.dodge_percent.value(part_level_max), 10.5F));
  const ChipRecord &ragdoll = *tables.chip(106);
  CHECK(ragdoll.track == 1 && ragdoll.lock_frames == 22);
  CHECK(near(ragdoll.knockdown_resistance.value(part_level_max), 18.5F));
  CHECK(near(tables.chip(101)->critical_percent.value(part_level_max), 10.0F));
}

TEST_CASE("the added CHIPs leave a table alone that does not end at the original's last") {
  auto parts = parts_with(3);
  CHECK(!add_chips(parts, added()));
  CHECK(parse_part_tables(parts).chips.size() == 3U);
}

TEST_CASE("each shop day offers one added cat, the families and personalities in turn") {
  std::string text = R"({"chip_lineups": [)";
  for (int day = 0; day < shop_days; ++day) {
    text += day == 0 ? "[]" : ",[]";
  }
  auto garage = json::parse(text + "]}");
  add_chip_lineups(garage, added());
  const auto &days = json::required(garage, "chip_lineups").array();
  for (int day = 0; day < shop_days; ++day) {
    const auto &offers = days[static_cast<std::size_t>(day)].array();
    CHECK(offers.size() == 1U);
    if (offers.size() != 1U) {
      return;
    }
    const int family = day % 3;
    const int personality = day / 3 % 5;
    CHECK(json::required(offers[0], "id").number() == original_chips + family * 5 + personality);
    CHECK(json::required(offers[0], "level").number() == 0.0);
  }
}
