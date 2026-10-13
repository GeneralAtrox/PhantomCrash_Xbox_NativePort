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
constexpr int added_records = 50; // ten families of five personalities
constexpr int first_specialist = 111;
constexpr int shop_days = 24;

const char *const original_chip = R"({"index": 0, "name": "Dog I", "inner_shape": 0,
  "inner_value": {"base": 200, "mid": 560, "top": 700, "knee": 60, "low_scale": -1},
  "track": 3, "outer_shape": 2,
  "outer_value": {"base": 200, "mid": 595, "top": 800, "knee": 60, "low_scale": -1},
  "lock_frames": 22, "chatter": 0})";

json::JsonValue added() {
  return DataRoots::shipped(PHANTOMCRASH_SOURCE_ROOT).document(added_chips_document);
}

// A parts table holding `count` chips and no weapons; the original's Rabbits
// (61-70) are named as theirs.
json::JsonValue parts_with(const int count) {
  std::string text = R"({"chips": [)";
  for (int index = 0; index < count; ++index) {
    std::string chip = original_chip;
    if (index >= 61 && index < 71) {
      chip.replace(chip.find("Dog I"), 5, index < 66 ? "Rabbit I" : "Rabbit II");
    }
    text += (index == 0 ? "" : ",") + chip;
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
  const int last = original_chips + added_records;
  CHECK(tables.chips.size() == static_cast<std::size_t>(last));
  const std::array<const char *, 10> names{"Cat V",    "Cat VI",     "Cat VII", "Fox I",
                                           "Bear I",   "Tortoise I", "Boar I",  "Hawk I",
                                           "Monkey I", "Meerkat I"};
  for (int index = original_chips; index < last; ++index) {
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
  const ChipRecord &maine_coon = *tables.chip(101);
  CHECK(maine_coon.lock_frames == 10);
  CHECK(near(maine_coon.critical_percent.value(part_level_max), 8.0F));
  CHECK(near(maine_coon.dodge_percent.value(part_level_max), 7.0F));
  const ChipRecord &ragdoll = *tables.chip(106);
  CHECK(ragdoll.track == 1 && ragdoll.lock_frames == 22);
  CHECK(near(ragdoll.knockdown_resistance.value(part_level_max), 18.5F));
  // The specialists, each best at one thing.
  CHECK(near(tables.chip(111)->critical_percent.value(part_level_max), 18.0F));
  CHECK(tables.chip(116)->face->image == "textures/chips/bear.png");
  CHECK(tables.chip(116)->track == 10);
  CHECK(near(tables.chip(121)->dodge_percent.value(part_level_max), 20.0F));
  CHECK(near(tables.chip(126)->knockdown_resistance.value(part_level_max), 32.0F));
  CHECK(tables.chip(126)->outer_shape == 1);
  CHECK(tables.chip(131)->lock_frames == 4 && tables.chip(131)->inner_shape == 1);
  const ChipRecord &mandrill = *tables.chip(136);
  CHECK(mandrill.inner_shape == 2);
  CHECK(near(mandrill.critical_percent.value(15), 5.0F));
  CHECK(near(mandrill.critical_percent.value(part_level_max), 5.5F));
  const ChipRecord &meerkat = *tables.chip(141);
  CHECK(meerkat.face->image == "textures/chips/meerkat.png");
  CHECK(meerkat.inner_shape == 0 && meerkat.outer_shape == 2 && meerkat.lock_frames == 6);
  CHECK(near(meerkat.outer_value.value(40), 999.0F));
}

TEST_CASE("the retune gives the original's Rabbits their jump-fighter stats") {
  auto parts = parts_with(original_chips);
  CHECK(add_chips(parts, added()));
  const PartTables tables = parse_part_tables(parts);
  for (const int index : {61, 70}) {
    const ChipRecord &rabbit = *tables.chip(index);
    CHECK(rabbit.lock_frames == 10);
    CHECK(near(rabbit.inner_value.value(35), 999.0F));
    CHECK(near(rabbit.outer_value.value(0), 500.0F));
    CHECK(near(rabbit.dodge_percent.value(part_level_max), 12.0F));
    CHECK(rabbit.inner_shape == 0); // the fixture's; the retune leaves shapes alone
  }
  CHECK(tables.chip(60)->lock_frames == 22); // not a Rabbit
}

TEST_CASE("the added CHIPs leave a table alone that does not end at the original's last") {
  auto parts = parts_with(3);
  CHECK(!add_chips(parts, added()));
  CHECK(parse_part_tables(parts).chips.size() == 3U);
}

TEST_CASE("each shop day offers one added cat and one specialist, in turn") {
  std::string text = R"({"chip_lineups": [)";
  for (int day = 0; day < shop_days; ++day) {
    text += day == 0 ? "[]" : ",[]";
  }
  auto garage = json::parse(text + "]}");
  add_chip_lineups(garage, added());
  const auto &days = json::required(garage, "chip_lineups").array();
  for (int day = 0; day < shop_days; ++day) {
    const auto &offers = days[static_cast<std::size_t>(day)].array();
    CHECK(offers.size() == 2U);
    if (offers.size() != 2U) {
      return;
    }
    const int cat = original_chips + day % 3 * 5 + day / 3 % 5;
    const int specialist = first_specialist + day % 7 * 5 + day % 5;
    CHECK(json::required(offers[0], "id").number() == cat);
    CHECK(json::required(offers[1], "id").number() == specialist);
    CHECK(json::required(offers[1], "level").number() == 0.0);
  }
}
