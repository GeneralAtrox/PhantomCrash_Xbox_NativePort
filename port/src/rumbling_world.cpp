#include "phantomcrash/rumbling_world.hpp"

#include "phantomcrash/json_fields.hpp"

#include <algorithm>
#include <stdexcept>

namespace phantomcrash {
namespace {

// A run of slow frames is not made up past this many steps an area.
constexpr int max_catch_up_steps = 8;
// A robot whose record gives it no armour still takes some shooting down.
constexpr float missing_armour = 1000.0F;
// The path's entry segments, at the arena's edge (enemy_path.hpp): the
// opponents already out at the start stand anywhere else on it.
constexpr int entry_kind = 0;

// Every weapon it still has is out of rounds, and none recharges.
bool out_of_ammo(const Combatant &robot) {
  bool armed = false;
  for (std::size_t slot = 0; slot < weapon_slots; ++slot) {
    if (robot.weapons[slot] == nullptr) {
      continue;
    }
    armed = true;
    const Magazine &magazine = robot.magazines[slot];
    if (!robot.lost[slot] && (!magazine.empty() || magazine.recharging())) {
      return false;
    }
  }
  return armed;
}

} // namespace

RumblingLive parse_rumbling_live(const json::JsonValue &document) {
  try {
    RumblingLive live;
    const auto &value = json::required(document, "live");
    live.tick_hz = value.number_or("tick_hz", live.tick_hz);
    live.arrival_seconds =
        static_cast<float>(value.number_or("arrival_seconds", live.arrival_seconds));
    if (live.tick_hz <= 0.0 || live.arrival_seconds < 0.0F) {
      throw std::runtime_error("tick_hz must be positive and arrival_seconds 0 or more");
    }
    const auto &stages = json::required(value, "stages").array();
    if (stages.size() != roster_areas) {
      throw std::runtime_error("stages needs a day and a night stage for each area");
    }
    for (std::size_t area = 0; area < roster_areas; ++area) {
      const auto &pair = stages[area].array();
      if (pair.size() != 2) {
        throw std::runtime_error("stages needs a day and a night stage for each area");
      }
      live.stages[area] = {pair[0].string(), pair[1].string()};
    }
    if (const json::JsonValue *paused = value.find("paused_screens");
        paused && paused->is_array()) {
      for (const json::JsonValue &name : paused->array()) {
        live.paused_screens.push_back(name.string());
      }
    }
    if (const json::JsonValue *ai = value.find("ai")) {
      live.ai = parse_drone_ai(*ai);
    }
    return live;
  } catch (const std::exception &error) {
    throw std::runtime_error(std::string("rumbling.json live: ") + error.what());
  }
}

RumblingWorld::RumblingWorld(const RegulationCheck check, const RumblingTables tables,
                             BattleSettings settings,
                             std::array<RumblingStage, roster_areas> stages, RumblingLive live,
                             const std::uint32_t seed)
    : check_(check), tables_(tables), settings_(std::move(settings)), stages_(std::move(stages)),
      live_(std::move(live)), random_(seed), epoch_((static_cast<std::int64_t>(seed) << 1) | 1) {
  settings_.ai = live_.ai;
}

float RumblingWorld::random_share() {
  return std::uniform_real_distribution<float>(0.0F, 1.0F)(random_);
}

void RumblingWorld::update(QuestSave &quest, const double seconds) {
  const bool taken_up = quest.live_epoch != epoch_;
  refresh_rosters(check_, tables_.contests, quest, [this] { return random_share(); }, taken_up);
  const double step_seconds = 1.0 / live_.tick_hz;
  for (int index = 0; index < roster_areas; ++index) {
    Area &area = areas_[static_cast<std::size_t>(index)];
    const AreaRoster &incoming = quest.rosters[static_cast<std::size_t>(index)];
    const bool redrawn = incoming.day != area.roster.day ||
                         incoming.contest != area.roster.contest ||
                         incoming.members.size() != area.roster.members.size();
    if (index != lent_ && (taken_up || redrawn)) {
      area.roster = incoming;
      build(index, quest);
    }
    if (area.battle) {
      area.clock += seconds;
      int steps = 0;
      for (; area.clock >= step_seconds && steps < max_catch_up_steps; ++steps) {
        step(index, quest, static_cast<float>(step_seconds), index != lent_);
        area.clock -= step_seconds;
      }
      if (steps == max_catch_up_steps) {
        area.clock = 0.0;
      }
    }
    quest.rosters[static_cast<std::size_t>(index)] = area.roster;
  }
  quest.live_epoch = epoch_;
}

void RumblingWorld::build(const int index, const QuestSave &quest) {
  Area &area = areas_[static_cast<std::size_t>(index)];
  area.battle.reset();
  area.robots.clear();
  area.player_kills.clear();
  area.clock = 0.0;
  ++area.changes;
  if (area.roster.members.empty()) {
    return;
  }
  const RumblingStage &stage = stages_[static_cast<std::size_t>(index)];
  area.battle =
      std::make_shared<Battle>(settings_, stage.path, static_cast<std::uint32_t>(random_()));
  Battle &battle = *area.battle;
  battle.set_stage(stage.collision);
  if (stage.objects) {
    battle.set_objects(*stage.objects);
  }
  // The player's place, empty until they come in (robot 0 is the player's).
  battle.add_robot("player", 0, {}, 0.0F, {}, 1.0F);
  battle.take_out(0);
  area.robots.resize(area.roster.members.size());
  for (std::size_t place = 0; place < area.roster.members.size(); ++place) {
    static_cast<void>(battle.add_drone("opponent", 1.0F));
    fit(index, place, quest);
    const LiveRobot &live = area.roster.members[place].live;
    const int robot = static_cast<int>(place) + 1;
    if (live.placed) {
      restore(battle, robot, live);
    } else {
      scatter(battle, index, robot);
    }
  }
}

void RumblingWorld::fit(const int index, const std::size_t place, const QuestSave &quest) {
  Area &area = areas_[static_cast<std::size_t>(index)];
  area.robots[place] = seat(*area.battle, index, place, quest);
}

RumblingRobot RumblingWorld::seat(Battle &battle, const int index, const std::size_t place,
                                  const QuestSave &quest) {
  const Area &area = areas_[static_cast<std::size_t>(index)];
  const RosterMember &member = area.roster.members[place];
  const int robot_index = static_cast<int>(place) + 1;
  RumblingRobot robot = rumbling_robot(tables_, quest, member.entrant,
                                       "live" + std::to_string(index) + "_" + std::to_string(place),
                                       settings_.movement, area.enemy_level_offset);
  std::array<std::string, weapon_slots> weapons{};
  for (std::size_t slot = 0; slot < weapon_slots; ++slot) {
    if (robot.weapons[slot]) {
      weapons[slot] = battle.add_weapon(*robot.weapons[slot]).name;
    }
  }
  // A team each: everyone out is against everyone else.
  battle.set_identity(robot_index, robot.name, robot_index,
                      robot.armour > 0.0F ? robot.armour : missing_armour);
  auto &brain = battle.robot(robot_index).ai;
  if (const auto *entrant = tables_.contests.entrant(member.entrant)) {
    brain.ability = entrant->level;
    brain.profile = entrant->ai_profile;
  }
  brain.entrant = member.entrant;
  brain.reset();
  battle.arm(robot_index, weapons);
  battle.set_tag(robot_index, robot.tag);
  // 0x0456b7 copies member/leader/champion to loadout +0x6c; the reserve
  // pool is +0x70 == 3. 0x04de50 maps them to yellow/blue/red/green.
  // RosterSource's four values index the editable HUD palette in that order.
  battle.robot(robot_index).marker_colour = static_cast<int>(member.source);
  battle.set_combat(robot_index, robot.combat);
  battle.set_lock_seconds(robot_index, robot.lock_seconds);
  battle.set_sight(robot_index, robot.sight);
  battle.set_movement(robot_index, robot.movement);
  battle.set_auto_respawn(robot_index, false);
  return robot;
}

void RumblingWorld::scatter(Battle &battle, const int area, const int index) {
  const EnemyPath &path = stages_[static_cast<std::size_t>(area)].path;
  std::vector<int> starts;
  for (std::size_t segment = 0; segment < path.segments.size(); ++segment) {
    if (path.segments[segment].kind != entry_kind) {
      starts.push_back(static_cast<int>(segment));
    }
  }
  if (starts.empty()) {
    return; // add_drone stood it at an entry
  }
  const auto pick = [this](const std::size_t count) {
    return std::min(static_cast<std::size_t>(random_share() * static_cast<float>(count)),
                    count - 1);
  };
  const int segment = starts[pick(starts.size())];
  const auto &points = path.segments[static_cast<std::size_t>(segment)].points;
  battle.place(index, points[pick(points.size())], random_share() * 2.0F * pi);
  battle.robot(index).path.start(path, segment);
}

void RumblingWorld::restore(Battle &battle, const int index, const LiveRobot &live) {
  if (live.down) {
    battle.take_out(index);
    return;
  }
  Combatant &robot = battle.robot(index);
  battle.place(index, {live.position[0], live.position[1], live.position[2]}, live.heading);
  battle.join_path(index);
  robot.hp = std::clamp(live.armour, 1.0F, robot.max_hp);
  robot.shield = live.shield;
  robot.kills = live.kills;
  for (std::size_t slot = 0; slot < weapon_slots; ++slot) {
    Magazine &magazine = robot.magazines[slot];
    if (robot.weapons[slot] != nullptr && magazine.limited() && live.ammo[slot] >= 0) {
      magazine.ammo = std::min(live.ammo[slot], magazine.max);
      magazine.shown = static_cast<float>(magazine.ammo);
    }
  }
}

void RumblingWorld::record(const Combatant &robot, LiveRobot &live) {
  if (!robot.alive) {
    return;
  }
  const Vec3 at = robot.mover.position();
  live.placed = true;
  live.position = {at.x, at.y, at.z};
  live.heading = robot.mover.heading();
  live.armour = robot.hp;
  live.max_armour = robot.max_hp;
  live.shield = robot.shield;
  live.kills = robot.kills;
  for (std::size_t slot = 0; slot < weapon_slots; ++slot) {
    const Magazine &magazine = robot.magazines[slot];
    live.ammo[slot] = robot.weapons[slot] != nullptr && magazine.limited() ? magazine.ammo : -1;
  }
}

void RumblingWorld::step(const int index, const QuestSave &quest, const float seconds,
                         const bool run) {
  Area &area = areas_[static_cast<std::size_t>(index)];
  Battle &battle = *area.battle;
  if (run) {
    battle.tick({}, {}, seconds);
  }
  for (std::size_t place = 0; place < area.roster.members.size(); ++place) {
    RosterMember &member = area.roster.members[place];
    const int robot_index = static_cast<int>(place) + 1;
    Combatant &robot = battle.robot(robot_index);
    if (battle.cooperative() && robot.piloted) {
      continue;
    }
    if (robot.alive && out_of_ammo(robot)) {
      battle.take_out(robot_index); // nothing left to fight with: it leaves
    }
    if (!robot.alive && !member.live.down) {
      member.live.down = true;
      member.live.arrival = live_.arrival_seconds;
      if (lent_ == index && robot.shot_by == 0) {
        area.player_kills.push_back(member);
      }
      robot.shot_by = -1;
    }
    if (member.live.down) {
      member.live.arrival -= seconds;
      if (member.live.arrival > 0.0F || robot.dying) {
        continue;
      }
      if (const auto next =
              draw_replacement(check_, tables_.contests, quest, area.roster,
                               static_cast<int>(place), [this] { return random_share(); })) {
        member = *next;
      }
      member.live = {};
      fit(index, place, quest);
      battle.bring_in(robot_index);
      battle.raise_shield(robot_index);
      ++area.changes;
    }
    record(robot, member.live);
  }
}

std::shared_ptr<Battle> RumblingWorld::lend(const int area, QuestSave &quest) {
  if (area < 0 || area >= roster_areas) {
    return nullptr;
  }
  update(quest, 0.0);
  lent_ = area;
  Area &lent = areas_[static_cast<std::size_t>(area)];
  lent.player_kills.clear();
  return lent.battle;
}

bool RumblingWorld::player_won() const {
  if (lent_ < 0) {
    return false;
  }
  return std::ranges::any_of(
      areas_[static_cast<std::size_t>(lent_)].player_kills,
      [](const RosterMember &kill) { return kill.source == RosterSource::champion; });
}

std::int64_t RumblingWorld::give_back(QuestSave &quest) {
  if (lent_ < 0) {
    return 0;
  }
  // The player's last 60 Hz tick may precede the world's next 30 Hz observation.
  step(lent_, quest, 0.0F, false);
  Area &area = areas_[static_cast<std::size_t>(lent_)];
  lent_ = -1;
  if (area.battle) {
    area.battle->take_out(0);
  }
  std::int64_t prize = 0;
  for (const RosterMember &kill : area.player_kills) {
    const int key = beaten_key(kill.team, kill.place);
    if (std::ranges::find(quest.beaten, key) == quest.beaten.end()) {
      quest.beaten.push_back(key);
    }
    const RosterCard card = roster_card(check_, tables_.contests, quest, kill.entrant, true);
    prize += card.bounty;
    quest.money = std::min(quest.money + card.bounty, money_limit);
  }
  area.player_kills.clear();
  return prize;
}

const RumblingRobot *RumblingWorld::robot(const int area, const int index) const {
  if (area < 0 || area >= roster_areas) {
    return nullptr;
  }
  const auto &robots = areas_[static_cast<std::size_t>(area)].robots;
  return index >= 1 && index <= static_cast<int>(robots.size())
             ? &robots[static_cast<std::size_t>(index - 1)]
             : nullptr;
}

int RumblingWorld::changes(const int area) const {
  return area >= 0 && area < roster_areas ? areas_[static_cast<std::size_t>(area)].changes : 0;
}

} // namespace phantomcrash
