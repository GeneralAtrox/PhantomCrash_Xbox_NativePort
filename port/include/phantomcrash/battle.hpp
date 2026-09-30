#pragma once

#include "phantomcrash/battle_weapons.hpp"
#include "phantomcrash/entry_shield.hpp"
#include "phantomcrash/enemy_path.hpp"
#include "phantomcrash/robot_mover.hpp"
#include "phantomcrash/stage_collision.hpp"
#include "phantomcrash/stage_doors.hpp"
#include "phantomcrash/stage_objects.hpp"
#include "phantomcrash/supply_drop.hpp"

#include <array>
#include <optional>
#include <map>
#include <random>
#include <string>
#include <vector>

namespace phantomcrash {

// The four weapon mounts, in the order the fire buttons use them: right
// trigger, left trigger, B, Y (Pepper's attack lesson, states 0x24-0x27).
enum class WeaponSlot { right_arm, left_arm, right_shoulder, left_shoulder };
inline constexpr int weapon_slots = 4;

// How drones fight when they do (the live RUMBLING's opponents, a port
// addition: the training ground's drones only walk). Each goes for the
// nearest robot of another team within engage_range, turns to face it,
// closes in or backs off to keep_distance and fires every weapon that has
// a lock.
struct DroneAi {
  bool fight{};
  float engage_range{150.0F};
  float keep_distance{40.0F};
  float distance_slack{10.0F}; // it holds still within this of keep_distance
  float walk{0.8F};            // the stick when closing in or backing off
  float turn_gain{2.0F};       // stick per radian off the target
  float face_degrees{40.0F};   // it walks only once facing this closely
};

[[nodiscard]] DroneAi parse_drone_ai(const json::JsonValue &value);

struct BattleSettings {
  ArenaBounds arena; // the walls while the stage has no collision
  MovementTuning movement;
  std::map<std::string, WeaponTuning> weapons;
  float robot_radius{2.0F}; // robots without a leg radius
  float robot_height{4.4F};
  float arena_wall_height{20.0F};
  float drone_respawn_seconds{4.0F};
  float drone_walk{0.6F};                   // the drones' stick when patrolling
  float drone_reach{3.0F};                  // a path point counts as reached this close
  float camouflage_seconds{12.0F};          // a full gauge lasts this long ...
  float camouflage_recharge_seconds{20.0F}; // ... and refills in this long
  float camouflage_fade_seconds{2.0F};      // it sweeps over the robot in this long
                                            // (60 ticks, states 1/2 and 5/6 of 0x074e80)
  float ammo_refill_seconds{};              // an empty weapon refills after this long (0: never)
  float idle_chatter_seconds{30.0F};        // standing with the pad untouched this long (0x08e480)
  EntryShield entry_shield;
  DroneAi ai;
};

[[nodiscard]] BattleSettings parse_battle_settings(const json::JsonValue &value);

struct Combatant {
  std::string name;
  std::string tag;       // the marker's name tag (its pilot's); empty for none
  bool leader{};         // its team's leader: the marker shows the leader icon
  int player_number{-1}; // versus mode's 1P-4P label (0-3), shown without a tag
  int team{};
  bool piloted{}; // a player controls it (robot +8 not -1): the first robot added
  bool rival{};   // on the side against the players (loadout +0x6c 2): the drones
  bool boss{};    // a boss (loadout +0x70 1): enters with its own scene (0x082cff)
  RobotMover mover;
  float radius{2.0F};       // the collision spheres' (0x22f424 per leg type)
  unsigned stage_contact{}; // stage_contact flags from the last tick
  float hp{100.0F};
  float max_hp{100.0F};
  bool alive{true};
  bool drone{};  // walks the enemy path and respawns
  bool frozen{}; // stands still (the lessons freeze the drones, 0x091ff4)
  float respawn_left{};
  bool auto_respawn{true}; // a drone shot down comes back by itself
  int shot_by{-1};         // who did most damage to it when it was last shot down
  std::array<const WeaponTuning *, weapon_slots> weapons{};
  std::array<float, weapon_slots> cooldown{};
  std::array<Magazine, weapon_slots> magazines{};
  // The robot each weapon would lock onto, how long it has been held and
  // the lock itself: missiles and grenades lock once held lock_seconds
  // (the AI chip's lock time), guns and melee weapons at once.
  std::array<int, weapon_slots> lock_candidates{-1, -1, -1, -1};
  std::array<float, weapon_slots> lock_held{};
  std::array<int, weapon_slots> locks{-1, -1, -1, -1};
  float lock_seconds{};
  // A homing missile's rounds leave one at a time (0x09b4ed): how many
  // are still to leave from each mount, and when the next one does.
  std::array<int, weapon_slots> queued_rounds{};
  std::array<float, weapon_slots> queue_left{};
  std::array<bool, weapon_slots> lost{}; // weapons a critical hit knocked off
  CombatStats combat;                    // its chip's and legs' (robot_parts.hpp)
  float invulnerable{};                  // seconds no hit hurts it (after a dodge)
  float shield{};                        // seconds its entry shield still lasts (+0x27d4)
  float shield_glow{};                   // the shield's glow, 0..peak (+0xac)
  Vec3 push;                             // knockback still carrying it, units a second
  std::vector<float> damage_from;        // by attacker, since it was last whole
  int kills{};
  float hit_flash{};
  bool camouflaged{};
  float camouflage{1.0F};   // gauge, 0..1
  float camouflage_sweep{}; // 0 seen .. 1 fully camouflaged, easing after `camouflaged`
  float ammo_share{1.0F};   // robot +0x110 (0x08248b), as of the last tick
  float idle_seconds{};     // standing untouched (robot +0x251c)
  PathFollower path;

  explicit Combatant(const MovementTuning &tuning) : mover(tuning) {}
  [[nodiscard]] Vec3 centre(float height) const {
    return mover.position() + Vec3{0.0F, height * 0.5F, 0.0F};
  }
  // Where the robot aims from: one leg radius above its feet, the middle
  // of its lower collision sphere (robot +0xb0 plus +0x14, 0x098bb7).
  [[nodiscard]] Vec3 aim_origin() const {
    return mover.position() + Vec3{0.0F, radius, 0.0F};
  }
};

// What the HUD's messages report (0x04da6c queues them per robot): a hit
// the robot landed (0x09d2ce), the robot shot down, or a robot it helped
// shoot down (both 0x07675e); and the chatter events raised for the robot
// (0x049470, battle_chatter.hpp). A drone that respawned reports it.
// A robot that takes a supply pickup reports it, and one that shoots the
// supply carrier down (`amount`: the credits of a money pickup, the
// carrier's bounty). The sounds hear a weapon fired (a round leaving, a
// strike swung: the robot's, from `slot` at `at`) and a shot or strike
// landing (the attacker's, at `at`: on a robot, a wall or the floor). A
// robot destroyed, by anyone or anything, is `destroyed` at its middle. An
// `effect` sets off effect group `slot` at `at` (the supply wreck's crash).
// A piloted robot that took damage this tick is `damaged` by the sum, its
// `amount` (the original's controller rumble, 0x07675e).
enum class BattleEventKind {
  shot_down,
  hit,
  bingo,
  chatter,
  respawned,
  supply,
  carrier_down,
  fired,
  impact,
  destroyed,
  effect,
  damaged
};

struct BattleEvent {
  int robot{};
  BattleEventKind kind{};
  int chatter{-1}; // the chatter event (chatter_event)
  float amount{};
  int slot{-1}; // fired: the weapon's mount (WeaponSlot); supply: the pickup's type
  const WeaponTuning *weapon{};
  Vec3 at;
  bool on_robot{}; // impact: it struck a robot
  std::optional<StageHit> surface;
};

// The player's buttons for weapons this tick.
struct FireControl {
  std::array<bool, weapon_slots> fire{}; // held
  bool camouflage{};                     // pressed: toggles Optic Camouflage
};

// A battle on one stage: robot 0 is the player; the others are drones
// walking the stage's enemy path, respawning when destroyed.
class Battle final {
public:
  Battle(BattleSettings settings, EnemyPath path, std::uint32_t seed = 1);
  // Robots point into the settings' weapons: no copies.
  Battle(const Battle &) = delete;
  Battle &operator=(const Battle &) = delete;

  // Adds a robot; `weapons` names entries of the settings' weapons (empty
  // for none). Returns its index.
  int add_robot(const std::string &name, int team, Vec3 position, float heading,
                const std::array<std::string, weapon_slots> &weapons, float hp);
  // Adds a drone at the start of one of the path's entry segments.
  int add_drone(const std::string &name, float hp);
  // Adds `tuning` to the settings' weapons (replacing one of its name) for
  // robots to carry; the reference stays valid.
  const WeaponTuning &add_weapon(WeaponTuning tuning);
  // Robot `index` carries these weapons (settings' names; empty for none),
  // their magazines full.
  void arm(int index, const std::array<std::string, weapon_slots> &weapons);
  // Robot `index`'s name, team and armour (whole).
  void set_identity(int index, const std::string &name, int team, float hp);
  // Whether drone `index` comes back by itself once shot down.
  void set_auto_respawn(int index, bool automatic);
  // Drone `index` comes in whole at one of the path's entries.
  void bring_in(int index);
  // Robot `index` is out of the battle until brought back (not shot down).
  void take_out(int index);
  // Robot `index`, to restore or change its state.
  [[nodiscard]] Combatant &robot(int index) {
    return robots_.at(static_cast<std::size_t>(index));
  }

  void tick(const RobotControl &player, const FireControl &fire, float seconds);

  void set_drones_active(bool active);
  // Robot `index` moves with its own tuning (its leg part's stride speeds).
  void set_movement(int index, const MovementTuning &tuning);
  // Robot `index`'s collision radius (its leg part's).
  void set_radius(int index, float radius);
  // Robot `index`'s marker name tag.
  void set_tag(int index, std::string tag);
  // Robot `index`'s marker extras: the leader icon and a versus player number.
  void set_marker(int index, bool leader, int player_number);
  // Robot `index` is a boss (loadout +0x70 1): it enters with its own scene.
  void set_boss(int index, bool boss);
  // What robot `index`'s chip and legs give it in a fight.
  void set_combat(int index, const CombatStats &combat);
  // The stage's walls and floor in place of the settings' box arena.
  void set_stage(StageCollision stage);
  [[nodiscard]] const StageCollision &stage() const noexcept {
    return stage_;
  }
  // The gates' doors: robots push against them and open them (stage_doors.hpp).
  void set_doors(StageDoors doors);
  void set_objects(StageObjects objects) {
    objects_ = std::move(objects);
  }
  [[nodiscard]] StageObjects *objects() {
    return objects_ ? &*objects_ : nullptr;
  }
  [[nodiscard]] const StageObjects *objects() const {
    return objects_ ? &*objects_ : nullptr;
  }
  // The stage's supply drops (supply_drop.hpp); none without routes.
  void set_supply(SupplySettings settings);
  [[nodiscard]] const SupplyDrop *supply() const noexcept {
    return supply_ ? &*supply_ : nullptr;
  }
  [[nodiscard]] StageDoors *doors() noexcept {
    return doors_ ? &*doors_ : nullptr;
  }
  [[nodiscard]] const StageDoors *doors() const noexcept {
    return doors_ ? &*doors_ : nullptr;
  }
  // Weapons the player may fire (bit per WeaponSlot); the rest stay silent.
  void set_allowed_weapons(unsigned mask) noexcept {
    allowed_weapons_ = mask;
  }
  // Robot `index` locks missiles and grenades after holding a target this long.
  void set_lock_seconds(int index, float seconds);
  // Puts robot `index` `distance` ahead of the player, facing it (the
  // Optic Camouflage lesson, 0x08d09e).
  void place_facing_player(int index, float distance);
  // Stands robot `index` at `position` facing `heading`.
  void place(int index, Vec3 position, float heading);
  // Robot `index` walks a script next tick instead of following its
  // controls, until released (battle_scene.hpp).
  void script(int index, const ScriptedStep &step);
  void release(int index);
  // Robot `index` leaves the battle by a gate, not shot down; a drone
  // comes back as a shot-down one does (0x08a496, 0x047c9e).
  void withdraw(int index);
  // Drone `index` takes up the enemy path where it stands (after its entrance).
  void join_path(int index);
  // Robot `index` is shielded for `seconds` (entry_shield.hpp): the
  // settings' entry shield when none are given.
  void raise_shield(int index);
  void raise_shield(int index, float seconds);
  // Raises chatter `event` for robot `index` among this tick's events.
  void tell(int index, int event);

  [[nodiscard]] const std::vector<Combatant> &robots() const noexcept {
    return robots_;
  }
  [[nodiscard]] const Combatant &player() const {
    return robots_.front();
  }
  [[nodiscard]] const Armory &armory() const noexcept {
    return armory_;
  }
  [[nodiscard]] const BattleSettings &settings() const noexcept {
    return settings_;
  }
  // The player's current lock for `slot`'s weapon, or -1.
  [[nodiscard]] int player_lock(WeaponSlot slot) const;
  // The robot `slot`'s weapon is locking onto (or has locked), or -1, and
  // for how many seconds.
  [[nodiscard]] int player_lock_candidate(WeaponSlot slot) const;
  [[nodiscard]] float player_lock_held(WeaponSlot slot) const;
  // What happened during the last tick.
  [[nodiscard]] const std::vector<BattleEvent> &events() const noexcept {
    return events_;
  }
  // Robots destroyed since the battle began.
  [[nodiscard]] int destroyed() const noexcept {
    return destroyed_;
  }
  // Where a robot's shots leave from and where it aims (a unit vector).
  [[nodiscard]] Vec3 muzzle(const Combatant &robot, WeaponSlot slot) const;
  [[nodiscard]] Vec3 aim(const Combatant &robot) const;

private:
  [[nodiscard]] std::vector<Target> targets() const;
  [[nodiscard]] int find_lock(const Combatant &robot, const WeaponTuning &weapon) const;
  [[nodiscard]] RobotControl drone_control(Combatant &drone);
  // DroneAi (battle_ai.cpp): the robot a drone goes for, or -1; how it
  // moves toward it; which of its weapons it fires.
  [[nodiscard]] int ai_target(const Combatant &drone) const;
  [[nodiscard]] RobotControl ai_control(Combatant &drone, int target);
  [[nodiscard]] static FireControl ai_fire(const Combatant &drone);
  void fire_drones(float seconds);
  void update_lock(Combatant &robot, std::size_t slot, float seconds);
  void fire_weapons(int index, const FireControl &fire, float seconds);
  void fired(int index, std::size_t slot, const WeaponTuning &weapon);
  void apply_hits(const std::vector<WeaponHit> &hits);
  // The rules of 0x0988e1: a hit's damage after dodges, criticals and
  // hits from behind; it may knock off a weapon. 0 when it does nothing.
  [[nodiscard]] float hit_damage(const WeaponHit &hit, Combatant &victim);
  void knock_off_weapon(Combatant &victim, Vec3 direction);
  void chatter(const Combatant &robot, int event);
  void armour_chatter(const Combatant &victim, float damage);
  void strike_chatter(const Combatant &attacker, const WeaponTuning &weapon);
  [[nodiscard]] static float ammo_share(const Combatant &robot);
  void watch_ammo(Combatant &robot);
  void watch_idle(Combatant &robot, const RobotControl &control, const FireControl &fire,
                  float seconds);
  void recover(Combatant &robot, float seconds) const;
  void shot_down(int victim);
  void collide(Combatant &robot);
  void separate();
  void respawn(Combatant &drone);
  void tick_supply(float seconds);
  void supply_explosion(Vec3 at);
  void take_supply(Combatant &robot, const SupplyTaken &taken);
  // The carrier and the pickups as shot targets, after the robots.
  void add_supply_targets(std::vector<Target> &out) const;
  void add_object_targets(std::vector<Target> &out) const;
  bool hit_object(const WeaponHit &hit);
  [[nodiscard]] int object_count() const;
  // A hit on one of them; false for a robot's.
  bool hit_supply(const WeaponHit &hit);

  BattleSettings settings_;
  StageCollision stage_;
  std::optional<StageDoors> doors_;
  std::optional<StageObjects> objects_;
  std::optional<SupplyDrop> supply_;
  EnemyPath path_;
  std::minstd_rand random_;
  std::vector<Combatant> robots_;
  Armory armory_;
  std::vector<BattleEvent> events_;
  unsigned allowed_weapons_{0xf};
  int destroyed_{};
  bool drones_active_{true};
  bool first_kill_told_{}; // chatter_event::first_kill is said once a battle
};

} // namespace phantomcrash
