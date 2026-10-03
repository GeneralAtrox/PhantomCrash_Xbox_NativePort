#pragma once

#include "phantomcrash/battle_events.hpp"
#include "phantomcrash/battle_hud_style.hpp"
#include "phantomcrash/battle_settings.hpp"
#include "phantomcrash/entry_shield.hpp"
#include "phantomcrash/enemy_path.hpp"
#include "phantomcrash/robot_mover.hpp"
#include "phantomcrash/shown_armour.hpp"
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
class RobotCombat;

// The active player's camera; opponents use their weapon's 3D aim cone.
// The fire control's reticle on the view, in normalised device
// coordinates (x right, y up): where it has tracked to (robot +0x108,
// +0x10c, 0x04f6a0) and the half sizes of its inner and outer boxes.
struct LockBoxes {
  std::array<float, 2> centre{};
  std::array<float, 2> inner_half{};
  std::array<float, 2> outer_half{};
};

struct TargetView {
  Mat4 view_projection;
  Vec3 eye;
  std::optional<LockBoxes> boxes; // none: the weapons' lock cones stand in
};

// The four weapon mounts, in the order the fire buttons use them: right
// trigger, left trigger, B, Y (Pepper's attack lesson, states 0x24-0x27).
enum class WeaponSlot { right_arm, left_arm, right_shoulder, left_shoulder };
inline constexpr int weapon_slots = 4;

struct Combatant {
  std::shared_ptr<RobotCombat> pose;
  bool pose_updated{};
  std::string name;
  std::string tag;       // the marker's name tag (its pilot's); empty for none
  bool leader{};         // its team's leader: the marker shows the leader icon
  int player_number{-1}; // versus mode's 1P-4P label (0-3), shown without a tag
  int team{};
  // HUD palette override; -1 follows the combat team (training/versus).
  // RUMBLING uses roster status independently of its free-for-all allegiance.
  int marker_colour{-1};
  std::array<bool, weapon_slots> firing{}; // accepted held triggers, for sustained weapon audio
  bool piloted{}; // a player controls it (robot +8 not -1): the first robot added
  bool rival{};   // on the side against the players (loadout +0x6c 2): the drones
  bool boss{};    // a boss (loadout +0x70 1): enters with its own scene (0x082cff)
  RobotMover mover;
  float radius{2.0F};              // the collision spheres' (0x22f424 per leg type)
  unsigned stage_contact{};        // stage_contact flags from the last tick
  int floor_surface{-1};           // the stage surface it stands over (-1: none, or a prop)
  std::optional<Vec3> last_ground; // where it last stood with stage ground under it
  float fence_contact_left{};      // delay until another force-field contact pulse
  float hp{100.0F};
  float max_hp{100.0F};
  bool alive{true};
  bool dying{}; // no longer targetable, but still playing the death motion
  float death_seconds{};
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
  SightBoxes sight; // its chip's boxes, for locking without a drawn reticle
  // The mount whose melee weapon is striking with its blade armed, or -1
  // (0x09c337; the attacker's +0x25a5).
  int strike_slot{-1};
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
  float hit_flash{};        // seconds of white left (hit_flash_seconds)
  ShownArmour shown_armour; // the gauges' armour (+0x50)
  bool camouflaged{};
  float camouflage{1.0F};   // gauge, 0..1
  float camouflage_sweep{}; // 0 seen .. 1 fully camouflaged, easing after `camouflaged`
  bool camouflage_broken{}; // a hit turned it off (0x075444): it fades out faster
  float ammo_share{1.0F};   // robot +0x110 (0x08248b), as of the last tick
  float idle_seconds{};     // standing untouched (robot +0x251c)
  PathFollower path;
  RobotAiState ai;

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

// The player's buttons for weapons this tick.
struct FireControl {
  std::array<bool, weapon_slots> fire{}; // held
  bool camouflage{};                     // pressed: toggles Optic Camouflage
};

// Tools > Cheats for the piloted robots (a port addition; off by default).
struct BattleCheats {
  bool never_die{};     // armour stops at 1
  bool infinite_ammo{}; // weapons stay full
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
  // External arrivals survive until the scene handles them, across battle ticks.
  [[nodiscard]] std::vector<int> take_arrivals();
  // Robot `index` is out of the battle until brought back (not shot down).
  void take_out(int index);
  // Robot `index`, to restore or change its state.
  [[nodiscard]] Combatant &robot(int index) {
    return robots_.at(static_cast<std::size_t>(index));
  }

  void tick(const RobotControl &player, const FireControl &fire, float seconds);
  void select_player(int index) {
    player_index_ = index;
  }
  [[nodiscard]] int player_index() const {
    return player_index_;
  }
  void set_cooperative(bool value) {
    cooperative_ = value;
  }
  [[nodiscard]] bool cooperative() const {
    return cooperative_;
  }
  // VS: every piloted robot has its own controls (set_player_control) and,
  // unlike co-op, they damage each other.
  void set_versus(bool value) {
    versus_ = value;
  }
  [[nodiscard]] bool versus() const {
    return versus_;
  }
  void set_player_control(int index, RobotControl control, FireControl fire) {
    controls_[index] = {control, fire};
  }

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
  [[nodiscard]] RobotWrecks &wrecks() {
    return wrecks_;
  }
  [[nodiscard]] const RobotWrecks &wrecks() const {
    return wrecks_;
  }

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
  void set_cheats(const BattleCheats cheats) noexcept {
    cheats_ = cheats;
  }
  // Robot `index` locks missiles and grenades after holding a target this long.
  void set_lock_seconds(int index, float seconds);
  void set_sight(int index, const SightBoxes &boxes);
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
    return robots_.at(static_cast<std::size_t>(player_index_));
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
  void set_player_target_view(std::optional<TargetView> view) {
    target_views_[player_index_] = std::move(view);
  }
  // Shared by the tracking reticle and weapon acquisition/lock maintenance.
  [[nodiscard]] bool target_visible(const Combatant &observer, const Combatant &target) const;

private:
  [[nodiscard]] bool clear_target_sight(Vec3 from, Vec3 to) const;
  [[nodiscard]] std::vector<Target> targets() const;
  // Where the robot locks: the drawn reticle's boxes, or boxes around its
  // sight seen through its camera, or through its own eyes (sight_view).
  [[nodiscard]] std::optional<TargetView> sight_view(const Combatant &robot) const;
  // The robot to lock `weapon` on `slot` onto: the nearest within its lock
  // range inside the inner box (arms) or the outer box (shoulders and
  // melee); or `held` while it stays within the weapon's hold.
  [[nodiscard]] int find_lock(const Combatant &robot, const WeaponTuning &weapon, std::size_t slot,
                              int held = -1) const;
  [[nodiscard]] RobotControl drone_control(Combatant &drone, float seconds);
  [[nodiscard]] RobotControl ai_control(Combatant &drone, float seconds);
  void ai_sense(Combatant &drone, float seconds);
  void ai_move(Combatant &drone, float seconds);
  void ai_route(Combatant &drone, float seconds);
  void ai_pickup(Combatant &drone, float seconds);
  void ai_weapons(Combatant &drone, float seconds);
  void ai_reactions(Combatant &drone, float seconds);
  void ai_jump(Combatant &drone, float seconds); // 0x05949b's jump at a target off the sight
  [[nodiscard]] int ai_lock_target(const Combatant &drone) const; // 0x04f6a0, robot +0x130
  [[nodiscard]] FireControl ai_fire(const Combatant &drone) const;
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
  float stage_floor(Combatant &robot) const;
  // `position` raised onto the ground under a robot standing there, which
  // becomes `mover`'s floor.
  [[nodiscard]] Vec3 placed(Vec3 position, float radius, RobotMover &mover) const;
  std::optional<Vec3> tracked_sight(const Combatant &robot, Vec3 crosshair) const;
  void shot_down(int victim);
  void collide(int index, float seconds);
  void pose_robot(int index, float seconds);
  void tick_death(int index, float seconds);
  void add_wreck_targets(std::vector<Target> &out) const;
  bool hit_wreck(const WeaponHit &hit);
  [[nodiscard]] Vec3 muzzle_direction(const Combatant &robot, WeaponSlot slot) const;
  [[nodiscard]] bool muzzle_blocked(const Combatant &robot, WeaponSlot slot) const;
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
  int player_index_{};
  bool cooperative_{};
  bool versus_{};
  std::map<int, std::pair<RobotControl, FireControl>> controls_;
  std::map<int, std::optional<TargetView>> target_views_;
  StageCollision stage_;
  std::optional<StageDoors> doors_;
  std::optional<StageObjects> objects_;
  RobotWrecks wrecks_;
  std::optional<SupplyDrop> supply_;
  EnemyPath path_;
  std::minstd_rand random_;
  std::vector<Combatant> robots_;
  Armory armory_;
  std::vector<BattleEvent> events_;
  std::vector<int> arrivals_;
  unsigned allowed_weapons_{0xf};
  BattleCheats cheats_;
  int destroyed_{};
  bool drones_active_{true};
  bool first_kill_told_{}; // chatter_event::first_kill is said once a battle
};

} // namespace phantomcrash
