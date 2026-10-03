# Original robot AI: recovered behaviour

Research and native baseline implementation: 2026-10-01.

## Scope and evidence

The recovery sections describe the European original executable. The final
section records how those findings are implemented in the native port. It is a first recovered description, not a complete
specification of every opponent or game mode. No original gameplay capture was
made during this investigation.

Primary evidence: `artifacts/europe/files/default.xbe`, SHA-256
`bdbe2c3eb2361395babfe6095d1a2da72f6581dccb34812b8d45e084c6c903c6`.
Addresses below are Xbox virtual addresses in that executable. Functions were
inspected with the existing Ghidra project and focused read-only decompilation.
Radare2 independently confirmed the NPC tick call at `0x0755d0`, the parameter
copies in `0x057820`, the indirect behaviour dispatch at `0x057ce2`, and the
table addresses and indexing in `0x05c680`. Capstone was used to locate callers.

All descriptive function, field and state names in this document are
researcher-assigned. They are not recovered original source symbols or SDK
symbol matches. Ghidra's `FUN_...` labels are generated names. x87 float-to-int
helper `0x1138ec` is not a random-number generator; some decompiled expressions
lose the floating-point calculation preceding it. Such expressions are not
treated as complete formulas here.

## What the original system does

The original has a stateful, probability-driven NPC controller, with authored
path following and additional reactive routines. It produces controls that are
consumed by the same robot movement machinery as player controls. It is more
elaborate than selecting the nearest enemy, walking to a fixed distance and
holding all weapon triggers.

### Controller pipeline

`0x0754eb` branches on robot `+0x08`: `-1` takes the NPC control path, while
player-controlled robots read input/replay controls. A living, enabled NPC calls
`0x057c71`, then proceeds through the shared robot update `0x075773`.

Each NPC has a `0x1dc`-byte controller record at
`0x471790 + robot_slot * 0x1dc`. `0x057863` initializes it. The main tick:

1. Refreshes target selection and target memory (`0x05b9c1`).
2. Updates health bands, weapon availability, target distances and counters
   (`0x058486`).
3. Invokes the current behaviour callback through controller `+0x14`.
4. Updates optical camouflage (`0x05ca2c`).
5. Runs additional reactions/overrides (`0x05949b`, except in a special route
   callback).
6. Evaluates incoming threats and requests evasive movement (`0x05d530`).
7. Scales and clamps four analogue control values to `[-1, 1]` and writes them
   to robot `+0x2f5c..+0x2f68`.
8. Processes four weapon channels (`0x05b360`), remembers current durability
   and advances counters.

### Per-NPC tuning and behaviour selection

During spawning, `0x080778` passes loadout byte `+0x58` and short `+0x5a` to
`0x057820`. They become controller `+0x3c` and `+0x40` respectively:

- **Ability parameter** (`+0x3c`, interpreted as a signed byte): influences
  target memory, control scaling, weapon decisions, detection and evasive
  reactions. `0x05f445` supplies it, with a forced value of 99 in scene mode 1.
  The identity of that scene mode is not established here.
- **Behaviour profile selector** (`+0x40`): chooses a 25-byte table at
  `0x206038 + profile * 25`. It is not simply a different movement speed.

`0x058486` computes five durability bands with
`clamp(current_durability * 5 / maximum_durability, 0, 4)`.
`0x05c680` uses a situation and the durability band to select a row of decision
thresholds. In the inspected function, situations include timer expiration,
having a target, a durability change, and the no-weapon-availability flag.
Later checks override earlier ones. The ordinary threshold table is `0x205e88`;
scene mode 1 substitutes `0x205f60`.

The five choices are evaluated sequentially through `0x05be18`, with state IDs
`[0, 1, 5, 3, 6]` from `0x2062a0`. These are **sequential chance checks**, not
five normalized weights to add together. There is also a chance to turn choice
0 into choice 1 and a fallback to the existing state.

`0x058846` changes state, installs a callback from `0x2061e0`, loads an
ability/state-dependent duration from `0x2062e8`, and establishes a four-second
transition guard using the game's timing-rate variable. This gives behaviours
persistence rather than making a fresh unrestricted choice every frame.

The recovered primary callback families are:

| State | Entry / running callback | Demonstrated actions; labels are descriptive |
|---|---|---|
| 0 | `0x058938` / `0x0589e7` | Random movement subchoice, movement-button requests, target turning and weapon requests; timed reevaluation. |
| 1 | `0x058ae3` / `0x058b66` | Authored route following, with target aiming and weapon requests alongside it. Initial state after controller setup. |
| 2 | `0x058cb0` / `0x058d0e` | Clears analogue movement initially; requests eligible weapons and reevaluates. Not selected by the five-choice table above. |
| 3 | `0x058d69` / `0x058e44` | Target-engagement family: multiple subchoices, aiming, movement/jump requests, firing, and a route-following subchoice. |
| 4 | `0x0593e3` / `0x059440` | Another stationary-initialization/weapon-request family. Its full distinction from state 2 is unresolved. |
| 5 | `0x059d62` / `0x059d8d` | Withdrawal-like movement: positive ability requests reverse movement while aiming/firing at the target; nonpositive ability takes a different forward/turn branch. |
| 6 | `0x059ea9` / `0x059ed8` | Special route-following behaviour that can still aim/fire; bypasses one common reaction routine. Do not call it an exit/retreat state without further evidence. |

Callbacks can change without changing the primary state number. This is not a
complete seven-state diagram: there are additional substates and overrides.

### Seeing and remembering opponents

`0x05b9c1` starts with candidates from a per-robot awareness structure, filters
by distance below 100 original world units, and has separate selection branches
for player-controlled and NPC-controlled opponents. It is not just the port's
nearest-hostile scan.

`0x05d11e` checks horizontal and vertical angular limits and a target eligibility
byte. Optical camouflage is explicitly considered through `0x074e2a`: detection
can be permitted by controller flags, proximity below 5 world units, or weapon
activity, using ability-dependent chance checks. Cloaked robots are therefore
not always categorically invisible to the original AI.

It also retains a target slot and a countdown instead of discarding every lost
candidate immediately. The inspected memory assignment is
`(ability + 127)^3 / 100000`, using integer arithmetic, followed by countdown
and checks that the remembered robot still exists and is alive.

The awareness structure is `*(globals + 0xee674 + robot * 0x30)` (accessor
`0x046844`). Its producer `0x04f152` writes, for every other robot `j`, a
0x20-byte record at `+0x48 + j * 0x20`: distance, bearing off the heading plus
twist, absolute elevation (`atan2(dy, horizontal)`), an in-frustum byte,
projected coordinates and a line-of-sight-blocked byte. The block test is a ray
(`0x0539ce`) from the robot's centre (`+0x454c`) plus the stage objects
(`0x06fd32`); the result for `j < self` is copied from the pair already
measured. Enemies (different team) are listed at `+0x114` (count `+0x12c`) and
bubble-sorted by distance.

`0x04f6a0` then picks the robot's lock (`+0x130`): the nearest listed enemy
that is visible, not cloaked, not blocked, in the frustum, closer than 75
(`0x19b55c`) and inside the sight's inner box (`0x0525b0`). It also counts, at
`+0x89c[j]`, the robots locking each robot.

Selection in `0x05b9c1` walks the enemies closer than 100 (`0x17ccd4`),
nearest first, in two passes:

1. A computer-controlled candidate (robot `+8 == -1`) is taken only when it is
   this robot's lock (`+0x130`). A player-controlled one is taken when
   `0x05d11e` sees it. The first match wins.
2. With no match, any candidate `0x05d11e` sees. Holding no target it takes
   the first (nearest). While a target is held, every seen candidate
   overwrites the choice, so the last (farthest) seen wins.

`0x05d11e` tests `|bearing| < ctrl[0x23]`, `|elevation| < ctrl[0x24]` and the
blocked byte, then camouflage through `0x074e2a`.

`0x05d2bd` (retaliation) runs after selection, only when the robot holds no
target (`+0x48 == 0`) and is not in the special route (`0x05f47f`: callback
`0x059ed8`). In order:

- **Attacker.** `0x05d4b7` walks the six robot slots of the robot's pending
  damage (`robot + 0x27a0 + j*4`, through `0x04678c`). That array collects the
  hits since the robot's last update; `0x07675e` applies and zeroes it every
  tick. It returns the first slot with damage whose class is below 2. A class-2
  slot (a ranker or champion) is returned only on a 10/255 chance; otherwise
  the walk carries on.
- **Chance.** With the attacker uncloaked (`0x074e2a`), (ability + 127)/255.
  With it cloaked, max(ability, 10)/255. Always when the robot's own class
  (`0x05ec7d`) is above 0.
- **Class.** `0x05ec7d` returns 2 for entrants 0, 1, 10 and 19 (the rankers).
  Otherwise it returns loadout `+0x6c`, which `0x0456b7` copies from the
  roster: member 0, leader 1, champion 2. `0x07675e` reads the same field
  (`== 2`) for the "hit by a rival" line. A reserve's is 0: `0x045270` seats
  the members (`0x045987`, class 0), the leader (1), the champion (2,
  `0x0459ed`) and then the reserve team `0x24b3a4` through `0x0462cd` with
  class 0; `0x045a68` copies that class unchanged and marks the reserve with
  `+0x70` = 3.
- **No side.** Neither `0x05d4b7` nor `0x05d2bd` asks which side the attacker
  is on; the original has no teams in a battle.
- **Supply chase.** While fetching a supply box (flag 4, record `+0x15c`) of
  kind 4 to 6, it carries on and keeps no target.
- **Result.** Otherwise it takes the attacker. Its memory becomes game rate × 2
  + ability × rate / 10 ticks (2 s + ability × 0.1 s at 30 Hz; not the
  sighting's (ability + 127)³ / 100000). It clears flags 0x1e (supply chase,
  jump), sets `+0x132` to rate × 8 and enters state 3. `+0x132` is the
  rankers' choice timer (below): a provoked ranker fights in the common states
  for 8 s before its own callback chooses again. Other robots never read it.

### Navigation and getting unstuck

`0x05efc1` / `0x05f0b3` attach a controller to authored paths using position,
facing direction, allowed segment types and grounded/airborne context.
`0x05a2a9` advances through curve segments and chooses among linked successors,
including a random branch and type exclusions. This matches the authored path
graph approach, not a recovered navmesh or global shortest-path planner.

`0x05a5dc` steers toward the path, handles airborne movement, and has timed
responses to obstruction/contact conditions: reverse movement, pauses,
alternative path selection, lateral evasive inputs and jump inputs. The jump
bit `0x04` is consumed by `0x08d7fa`; lateral bits `0x10/0x20` by `0x08d98c`.
Path segment types also affect speed and waiting behaviour.

`0x05eb80` periodically reacquires a route after a four-second counter unless
progress resets it. This is evidence of recovery machinery, not evidence that
it reliably escapes every corner, prop arrangement or ramp.

### Aiming and firing

`0x05c1fb` generates horizontal turning and vertical aiming controls from target
angles and the robot's turning capability. It does not simply teleport the
robot's heading to its target.

`0x05b139` requests weapons whose distance bands pass `0x05d28c`:
`minimum_range <= target_distance < maximum_range`. It also uses the ability
parameter and the count of active weapon requests.

`0x05b360` processes four weapon records with independent counters, probability
checks, weapon-type branches and additional target/lock eligibility flags.
It runs only for a held target (memory, not sight) that `0x05b992` finds
unblocked: the awareness record's line-of-sight byte. Above ability 0 the
target must also be in the robot's view (the record's frustum byte). The sight
angles of `0x05d11e` do not apply to firing.
It finally issues weapon commands through `0x0762e8`. Availability is refreshed
in `0x058486` / `0x05c04c`, and unavailability participates in state selection.
The complete meaning of every weapon subtype and timing field is unresolved;
the evidence supports timed/type-aware firing rather than claiming one universal
burst duration.

### Camouflage and threat reactions

`0x05ca2c` explicitly requests the same camouflage input bit (`0x08`) consumed
by the robot camouflage update. It uses on/off counters, probability gates,
target/threat flags and controller-specific conditions.

`0x05d530` scans several threat lists. It uses projectile positions, velocities,
distance, facing/dot products, ability-dependent thresholds and chance gates.
On a positive reaction it randomly requests one of the two lateral evasive
directions. One threat category is checked only every fifteenth controller
update. This is reactive evasion; no predictive trajectory optimizer was
recovered.

### Jumping at a target off the sight

`0x05949b` holds a second reaction besides the supply chase. At 15/255 a tick
it starts a jump (`0x059799`) when all of these hold:

- the robot is not in the special route (`0x057c71` skips `0x05949b` when
  `0x05f47f` holds);
- the quest's stage index (`+0x6104`) is not 2 or 3, which is RUMBLING area 1
  (area = index / 2);
- the robot is not in `0x06e331`'s zone 1;
- it carries no melee weapon (kind 6, `0x05ec15`);
- it is not airborne (motions 4 and 5);
- controller flag 2 is clear;
- it has a target 5 to 20 units away (`0x19b544`, `0x199f7c`), feet to feet;
- the target is more than 15 degrees (immediate `0x3e860a92`) off the sight
  direction (`+0x2688`).

Zone 1 is a fixed circle (centre 30.522, 59.594; radius 39.295;
`0x1b5a5c..0x1b5a64`). It covers ground above −2.5 outside the circle, or at
−0.5 and above inside it; zones 0 and 2 to 4 are lower floors. The circle
describes area 1: `0x059fb0` and `0x07973f`, the function's other callers,
consult it only when the stage index is 2 or 3. `0x05949b` skips area 1 outright
and then applies area 1's circle in every other area, where it means nothing in
particular. RUMBLING area 0's streets stand near −3, below it, so the jump
happens there.

The states, one controller tick each:

- **Press (`0x059799`).** Lets go of every stick (controller `+0x60..+0x6c`).
  It presses robot input bits 2 (held) and 4 (fresh), the bits the jump
  button (`+0x2f88`) sets in `0x0774ad`. `0x05b061` sets bit 1 of each
  weapon channel's flag word (controller `+0x94 + slot × 0x14 + 0x0a`) at
  chance (ability + 128)/255. Nothing in the XBE reads it: the only accesses
  are this write and the clears in `0x05bf22` and `0x05e1ec`; no code uses the
  channel's absolute addresses or `+0x9e/+0xb2/+0xc6/+0xda`, and no other
  `+0x94` record walk reads `+0x0a`.
- **In the air (`0x05981c`).** Without a target it returns to state 1.
  Otherwise it requests weapons (`0x05b139`), pushes forward at full stick
  (`+0x60 = 1.0`) and aims (`0x05c1fb`, flags 0xc) at the target's awareness
  record: where the target is, seen or not. It holds bits 2 and 4 in
  motion 4 (rising) and bit 2 in motion 5 (air). The controller tick zeroes
  the input bits first (`0x057c71`), so the fresh bit never reaches motion 5.
  In `0x0905af` a held jump keeps its full lift, and no descent latch fires.
- **Landed.** Any other motion lets go, sets the aim timer to 30 × 3.0
  (`0x17cce0`) ticks and moves to `0x059921`.
- **Aiming (`0x059921`).** Aims while it still holds a target, and counts
  down whether or not it does; then state 1. It never resets the forward
  stick, and the controller's sticks persist, so the robot keeps walking
  forward at full stick through the three seconds.

Quest `+0x618c == 1` (the attract demo and RESULT's backdrop battle) makes
`0x05f445` report ability 99 for every opponent. With it, `0x05949b` chases a
supply box of any kind, money included.

`0x05ddb5` / `0x05de0f` also install special behaviour callbacks for loadout
`+0x64` values 0, 1, 10 and 19. These use durability bands and weapon-policy
flags. `0x0456b7` copies the roster entrant ID to loadout `+0x64`, so these are
entrant IDs. The native implementation reads those same IDs from the roster.

`0x05949b` calls `0x05ddb5` first, so the ranker's choice runs every tick after
the state callback, except in the special route (`0x05f47f`). `0x05de0f`
counts `+0x132` down (held to ±20000). When it reaches 0 with no flag 0x1e
set, it sets the band's weapon policy and chooses: the attack callback
`0x05e660` when it holds a target and the band is below full (full: 40/255),
else the route callback `0x05e494`, or `0x05e660` when the count `+0x12c`
is odd (not reproduced). Past rate × 180 on that count with a target it also
turns defensive. It
writes rate × 4 for the attack and rate × 8 for the route, but both callbacks
write rate × 8 as they start the next tick, so either choice holds 8 s.
`0x05e292` and `0x05ea6f` fall back to `0x05e494` (and 8 s) when the target
is lost. Its cases 1 and 2 (`0x05e157`, 4 s; `0x05e363`, 5 s) are unreachable:
the choice is only ever 0 or 3.

## Important limits and corrections

- Loadout `+0x5c` was previously labelled an AI index. The spawn/input path
  disproves that interpretation: negative values select NPC/inactive cases;
  nonnegative values are used for player/controller ownership. `0x080778`,
  `0x0825df` and `0x046f81` support the correction. Behaviour tuning comes from
  `+0x58/+0x5a`.
- Robot `+0x34ac` and callback table `0x249848` concern movement sound families,
  not tactical AI. They must not be used as evidence of behaviour profiles.
- Initialization order needs further recovery before numerical tuning can be
  treated as the effective settings of a named opponent. In the inspected spawn
  path, `0x057863` clears and initializes the controller before `0x057820` copies
  its ability/profile parameters. Several derived settings are computed during
  that initialization. Their subsequent refresh lifecycle is not established.
  Direct per-tick reads of the parameters are established.
- Some original branches use fixed six-slot loops. Their assumptions cannot be
  copied blindly into the port's larger robot populations.
- Follow-up inspection of `0x0678f8` identifies `0x9668a0`, the existing supply
  pickup array (three 0x28-byte records). `0x05949b` / `0x059a1d` divert toward
  the nearest falling/landed pickup within 25 units, without an enemy target.
  Ability >=31 rejects kinds 0..3 (money); the chase expires after five seconds.
- No squad coordination, cover scoring or long-term tactical planning was
  established in the inspected routines. This is a recovery limit, not proof
  that no related behaviour exists elsewhere.

## What this tells us about the port

The current `port/src/battle_ai.cpp` finds the nearest eligible opponent, turns
and moves forward/backward around one desired distance, then fires valid locks
after range and obstruction checks. The recovered original adds substantial
behaviour on top: persistent probabilistic states, ability/profile data, target
memory, angle-limited detection, conditional camouflage detection, vertical aim,
weapon-specific decisions, jump/evasion requests and route recovery.

The original still plausibly feels poor because it relies heavily on authored
routes, local steering, timers and random choices. That is an inference about
the recovered design, not a measured rating of its gameplay. A modern tactical
planner would be a change in design, not a description of what has been recovered.

The visible implementation is small fixed controller records, table lookups,
bounded opponent/weapon loops and geometry checks; it does not reveal an
expensive global planning system. However, **original AI CPU cost has not been
profiled**, and route/threat processing prevents concluding that every tick is
constant-cost. No performance number is claimed.

## Native baseline implementation

The live RUMBLING now uses `robot_ai` and the `battle_ai` adapters instead of
nearest-enemy / fixed-distance steering. Training's frozen and scripted drones
retain their lesson controls. `port/tests/battle_ai_tests.cpp` covers the lock
rule, the second pass, retaliation and the provoked ranker's hold, the jump
reaction, the showcase ability and the port options.

`data/world/robot_ai.json` contains the twelve original 25-byte profiles,
42 five-byte decision rows, state order, search weights, and 100 ability rows.
Regenerate it with:

```powershell
$env:PYTHONPATH = 'src'
python -m phantomcrash_research.robot_ai_tables --xbe artifacts/europe/files/default.xbe --output data/world/robot_ai.json
```

The extraction addresses are recorded in that module. The names remain
research labels. `mods/data/world/robot_ai.json` can override the tuning.
Entrants now expose `ai_profile`; existing extractions using `word_4` also load.
`0x0456b7` copies the entrant level into ability separately from the part-level
adjustments, so New Game Plus part increases do not automatically raise ability.

Implemented policies:

- State decisions run at 30 Hz in both visible and offscreen battles. Health,
  target, elapsed-state and ammunition situations select the recovered rows.
- Per-entrant ability controls sight angles, target memory, probability and
  firing delays. Targets can be lost, searched for, or reacquired. Current
  collision rays gate visibility and firing.
- Retaliation follows `0x05d2bd` / `0x05d4b7`. It acts only with no target
  held, blaming the first robot that hurt the drone since its last AI tick
  (the change in `damage_from`). Class comes from `ranker_entrants` and the
  roster source (`marker_colour`: member 0, leader 1, champion 2). The chances,
  the supply exception and the 2 s + ability × 0.1 s memory are `ranker_class`
  / `retaliation_*` parameters.
- Firing follows `0x05b360`'s gate: a remembered target with a clear line of
  sight, in the robot's view above ability 0. The weapon lock and
  `target_visible` (cloak detection) still apply.
- The attract demo and RESULT's backdrop set `DroneAi::showcase`: every
  opponent plays at `showcase_ability` (99) and chases any supply box.
- Selection follows `0x05b9c1`'s two passes: a computer rival must be the
  robot's lock (`Battle::ai_lock_target`, after `0x04f6a0`, with `lock_range`
  and the sight's inner box). A player's robot must be seen. The second pass
  keeps the last seen while a target is held, as the original does. The
  elevation limit is absolute, as in `0x04f152`.
- The jump at a target off the sight follows `0x05949b` / `0x059799` /
  `0x05981c` / `0x059921`, including the stick release on the press and the
  forward push through the aim. Its range, angle, chance, aim time, skipped
  area and `0x06e331`'s circle are `jump_reaction_*` / `high_ground_*`
  parameters.
- The route, engagement, stationary and withdrawal families produce native
  robot controls. Existing ENPT polylines, slower route kinds, pauses and
  obstacle recovery drive movement; there is no new global path planner.
- Weapons use requests, reaction delays and probability checks. Requests remain
  pending until the native firing pose is ready and a shot is acknowledged.
  This is needed because a one-tick trigger otherwise expires during arm raising.
- Camouflage uses the recovered ability curves and the port's gauge, drain and
  recharge rules. Cloaked opponents are detectable by the ability checks.
- Incoming projectiles can provoke dodging and camouflage. The ranker entrant
  cases use their health-dependent route/approach, close dodge and weapon policies.
- With no enemy target, NPCs can approach supply pickups within 25 units and
  jump for an overhead box. Ability >=31 skips money, and a chase expires after
  five seconds or when the box is collected/destroyed.
- Death, removal and respawn reset runtime AI state; roster ability/profile stay
  attached to the opponent. Scripted door entrances keep control of movement.

### Native integration limits

This is a recovered-behaviour baseline, **not a complete executable-equivalent
translation**. In particular:

- Derived settings use the assigned entrant ability. The original constructor /
  setter ordering described above remains unresolved; this choice avoids caching
  zero-ability curves for every opponent without evidence of that intention.
- Visibility, projectile threats, usable weapon ranges and movement outputs use
  the existing native collision, weapons and locomotion systems. Perception is
  computed on demand from the same quantities `0x04f152` records (distance,
  bearing, elevation, frustum, line of sight); no per-robot record table is
  kept. Native weapon readiness is an explicit adapter.
- A reserve's class is 0, as in the original. The player's is taken as 0.
- Retaliation asks no side, as in the original. No weapon hurts a teammate in
  the port, so `damage_from` never names one; a teammate target would also be
  dropped by sensing on the next tick.
- A provoked ranker (`retaliation_ranker_hold_seconds`, `+0x132`) runs the
  common states until its choice timer runs out. The timer only runs while
  `ai_move` does (not during a supply chase or the jump); the original's runs
  in every tick but the special route's.
- `0x05b061`'s per-weapon flag is not reproduced: nothing in the XBE reads it.
- Route recovery and search timing include native bridge parameters alongside
  recovered constants. Special movement callbacks are expressed through native
  movement controls; they do not overwrite the player's camera or teleport NPCs.
- The supply candidate is now resolved and implemented: idle opponents can chase
  nearby supply boxes, with the original money/ability restriction and timeout.
  Pickup collection uses the existing collision handler. The original candidate
  marker write appears to use the finished scan index; no out-of-bounds write is
  reproduced in native code. Replay does not rerun this controller.

### Port options

Four switches in `live.ai` (`world/rumbling.json`, overridable from a mod)
change original quirks. All are off by default, so the defaults stay the
original's:

| Option | Effect |
| --- | --- |
| `nearest_fallback_target` | `0x05b9c1`'s second pass keeps the nearest seen rival instead of the last seen while a target is held. |
| `jump_keeps_target` | Target memory does not run down during the jump and the aim after it. Weak pilots (0.68 s of memory at ability 0) otherwise lose the target mid-air and go back to their route. |
| `rivals_by_sight` | A computer rival is taken on sight like a player's robot, not only once locked. Opponents then fight each other as readily as they go for the player. |
| `jump_any_floor` | Area 1's floors (`0x06e331`) no longer block the jump in other areas, where they describe no real ground. |

Set `live.ai.trace` in `world/rumbling.json` to true for per-opponent identity and
periodic state/target/health/request/actual-attack/dodge/recovery diagnostics.
It is disabled by default. Release build and bounded native battle runs were
used for validation; no original xemu behaviour comparison was performed.


Validation on 2026-10-01: the Release game target built successfully.
A 3,600-frame native rental battle (60 simulated seconds, three live areas)
exited normally, initialized 27 opponent lifecycles, and logged actual attacks,
durability loss, state/target changes, dodges and a route recovery. Weapon audio
received 193 submissions. No pickup chase happened in that sampled run, so its
selection/collection flow is implemented but not yet observed end-to-end there.
These are runtime observations, not an original-game parity or CPU benchmark.
