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

The awareness structure contains gating flags used by acquisition and firing.
Their complete producers, including exact wall-occlusion, team and spawn-state
semantics, have not been traced in this pass. This is not proof that the original
could never acquire or remember someone behind a wall.

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

`0x05ddb5` / `0x05de0f` also install special behaviour callbacks for loadout
`+0x64` values 0, 1, 10 and 19. These use durability bands and weapon-policy
flags. `0x0456b7` copies the roster entrant ID to loadout `+0x64`, so these are
entrant IDs. The native implementation reads those same IDs from the roster.

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
retain their lesson controls. No new tests were added.

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
  firing delays. Targets can be lost, searched for, or reacquired. Damage can
  provoke retaliation. Current collision rays gate visibility and firing.
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
  the existing native collision, weapons and locomotion systems. The original's
  separate perception producer records and every type-specific reactive branch
  have not been reproduced. Native weapon readiness is an explicit adapter.
- Route recovery and search timing include native bridge parameters alongside
  recovered constants. Special movement callbacks are expressed through native
  movement controls; they do not overwrite the player's camera or teleport NPCs.
- The supply candidate is now resolved and implemented: idle opponents can chase
  nearby supply boxes, with the original money/ability restriction and timeout.
  Pickup collection uses the existing collision handler. The original candidate
  marker write appears to use the finished scan index; no out-of-bounds write is
  reproduced in native code. Replay does not rerun this controller.

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
