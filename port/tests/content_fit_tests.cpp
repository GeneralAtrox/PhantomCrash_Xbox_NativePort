// One save for base game and S.L.A.I.: what the loaded content cannot show
// is set aside, kept in the save and given back.

#include "test_support.hpp"

#include "phantomcrash/content_fit.hpp"
#include "phantomcrash/vs_scoobee.hpp"

using namespace phantomcrash;

namespace {

// The base game's counts, and S.L.A.I.'s, which append to every list.
constexpr ContentLimits base_limits{16, 43, 76, 69, 96, 107, 57, 206};
constexpr ContentLimits slai_limits{26, 78, 123, 117, 96, 107, 115, 436};

// Hangar 0 a base robot, hangar 1 a S.L.A.I. one (ridden), a loose S.L.A.I.
// arm and a S.L.A.I. paint owned.
Garage mixed_garage() {
  Garage garage = new_game_garage(3);
  const auto fit = [&garage](const int hangar, const PartKind kind, const int id) {
    return garage.add_stock({kind, id, 0, {}, hangar});
  };
  Hangar &base = garage.hangars[0];
  base.parts = {fit(0, PartKind::body, 3), fit(0, PartKind::legs, 5), -1, -1, -1, -1,
                fit(0, PartKind::chip, 1)};
  base.name = "BASE";
  Hangar &slai = garage.hangars[1];
  slai.parts = {
      fit(1, PartKind::body, 20), fit(1, PartKind::legs, 5), fit(1, PartKind::arm, 2), -1, -1, -1,
      fit(1, PartKind::chip, 2)};
  slai.modules = {fit(1, PartKind::module, 4)};
  slai.name = "SLAI";
  garage.add_stock({PartKind::arm, 100, 0, {}, -1});
  garage.add_stock({PartKind::shoulder, 3, 0, {}, -1});
  garage.riding = 1;
  garage.paints = {0, 60};
  return garage;
}

} // namespace

TEST_CASE("S.L.A.I. robots and parts are set aside while S.L.A.I. is off") {
  const Garage original = mixed_garage();
  Garage garage = original;
  fit_garage(garage, base_limits);

  CHECK(garage.hangars[0] == original.hangars[0]);
  CHECK(garage.locked(1));
  CHECK(!garage.hangars[1].owned);
  CHECK(garage.hangars[1].empty());
  CHECK(garage.riding == -1);
  CHECK(garage.set_aside.riding == 1);
  CHECK(garage.set_aside.hangars.at(1) == original.hangars[1]);
  // The S.L.A.I. hangar takes its base legs with it; the loose S.L.A.I. arm
  // goes too, the loose base shoulder stays.
  CHECK(garage.set_aside.stock.size() == 6);
  CHECK(garage.stock.size() == 4);
  CHECK(garage.paints == std::vector<int>{0});
  CHECK(garage.set_aside.paints == std::vector<int>{60});

  // Set-aside entries keep their stock indices.
  CHECK(garage.free_stock() == original.free_stock());
  const int added = garage.add_stock({PartKind::chip, 0, 0, {}, -1});
  CHECK(!garage.set_aside.stock.contains(added));
  garage.stock.erase(added);

  // Fitting again changes nothing.
  Garage again = garage;
  fit_garage(again, base_limits);
  CHECK(again == garage);
}

TEST_CASE("a set-aside garage survives the save and comes back with S.L.A.I.") {
  const Garage original = mixed_garage();
  Garage garage = original;
  fit_garage(garage, base_limits);
  Garage loaded = garage_from_json(json::parse(json::write(garage_to_json(garage))));
  CHECK(loaded == garage);

  fit_garage(loaded, slai_limits);
  CHECK(loaded.set_aside.empty());
  CHECK(loaded.stock == original.stock);
  CHECK(loaded.hangars == original.hangars);
  CHECK(loaded.riding == 1);
  CHECK(loaded.paints == original.paints);

  // A garage with nothing set aside writes no set_aside member.
  CHECK(garage_to_json(original).find("set_aside") == nullptr);
}

TEST_CASE("a locked hangar keeps its robot when the player rides another meanwhile") {
  Garage garage = mixed_garage();
  fit_garage(garage, base_limits);
  garage.riding = 0;
  fit_garage(garage, slai_limits);
  CHECK(garage.riding == 0);
  CHECK(garage.hangars[1].name == "SLAI");
  CHECK(garage.set_aside.empty());
}

TEST_CASE("fitting needs every table") {
  Garage garage = mixed_garage();
  fit_garage(garage, ContentLimits{});
  CHECK(garage == mixed_garage());
  const auto parts = json::parse(R"({"bodies":[1,2],"legs":[1],"arm_weapons":[1],
    "shoulder_weapons":[1],"chips":[1,2,3]})");
  const auto tables = json::parse(R"({"modules":[1],"paints":[1],"emblems":[1,2]})");
  const ContentLimits limits = content_limits(parts, tables);
  CHECK(limits.valid());
  CHECK(limits.bodies == 2);
  CHECK(limits.chips == 3);
  CHECK(limits.emblems == 2);
}

TEST_CASE("a VS Scoobee with any S.L.A.I. part reads as damaged while S.L.A.I. is off") {
  PartTables parts;
  parts.bodies.resize(2);
  parts.legs.resize(2);
  parts.arm_weapons.resize(2);
  parts.shoulder_weapons.resize(2);
  parts.chips.resize(2);
  VsScoobee scoobee;
  scoobee.robot.parts[0] = RobotPart{1, 0};
  scoobee.robot.parts[1] = RobotPart{1, 0};
  scoobee.robot.parts[6] = RobotPart{1, 0};
  CHECK(!vs_scoobee_damaged(scoobee, parts));
  scoobee.robot.parts[3] = RobotPart{5, 0};
  CHECK(vs_scoobee_damaged(scoobee, parts));
}
