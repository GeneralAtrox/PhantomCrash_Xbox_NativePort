# The training ground, the tutorial battle and the 3D renderer

## Arena monitors

Gate TVs and the advertising ship show a live chase view of a random living
combatant, including the player and AI. The port switches every eight simulation
seconds, avoids immediate repeats when another fighter is available, and switches
immediately if its subject dies. With no living fighters, the screens go black.
`data/world/stages.json` controls `monitor.switch_seconds`, the camera and aspect,
and each stage's `monitor_nodes`. These name the gate quads independently of the
sky geometry that shares their original placeholder texture.

The feed renders the same battle snapshot, including robots, scenery, projectiles
and effects, without HUD or cockpit. It uses native viewport resolution and is
shared by every TV and split-screen view. TVs within the feed are black to prevent
recursive feedback. The renderer expands each panel's authored UV rectangle to
show the complete feed.

`arena_monitor.py` extracts the previously unloaded advertising ship models
210/211 and their textures, plus the stage flight curves selected at `0x066940`
and followed by `0x06a38d`. `tables/arena_monitor.json` contains the route, speed,
hinged panel groups and night rotors. The screen meshes are slots 1218/1222 by day
and 1233/1237 by night; `0x06aa07` replaces their atlas UVs for the framebuffer.
The native ship follows those curves and displays the shared battle feed.
Random subject cycling and continuous battle coverage replace the original
player-only view and ship advertisements as a requested port improvement.

Pepper's BEGINNERS CLUB lessons (FreePlay, basic controls, attack methods,
Optic Camouflage) start the tutorial battle, scene 42, on the training
ground. It is the port's first 3D screen: the player's Scoobee, posed from
its part skeletons and motions, walks the stage under a chase camera
against drones that patrol the stage's enemy path. Pepper's in-battle
messages lead through each lesson and return to her when it ends.

Addresses are virtual addresses in the European `default.xbe`. Asset names
such as `choco_mdl` and `12SENTOU.WAV` are the disc's own file names. Names
like "view layer", "batch", "pass", "hold" and the lesson goal names are
researcher-assigned; there are no game-source symbols.

## How the port runs it

- `data/ui/pepper_guide.json`: each lesson's `next` names a training screen.
- `data/world/training.json` maps those screens to lessons:

  | Screen | Mode | Course |
  |---|---|---|
  | `training_free` | 3 | none: FreePlay, until `back` or the exit through a gate |
  | `training_basic` | 0 | states 24-34 |
  | `training_attack` | 1 | states 35-41 |
  | `training_optic` | 2 | states 42-44 |

  The lesson handler at 0x2928c stores this mode in the save at +0x10f10.
  The same file holds the stage, the music (scene 42 plays track 5,
  `12SENTOU.WAV`), the robots' parts, where the battle returns to, and the
  `battle` section: courses, buttons, movement, weapons, arena, drones and
  the chase camera.
- `data/world/stages.json`: each stage's glTF, the nodes drawn as its sky,
  its collision, its lighting and its fog. The lighting and fog are the
  stage's first environment preset (see [The stage's environment](#the-stages-environment)).
- Converted by `convert-robots` (not committed): `tables/lessons.json`
  (Pepper's messages), `enepath/choco_es_01_enepath.json` (the drones'
  path), the robot catalogue, skeletons, part models, paints and motions.
- Code, core (no SDL): `battle` (robots, drones, collision) and
  `battle_fire` (locks, firing, hits and the HUD's events),
  `battle_weapons` (weapons, ammunition), `robot_parts` (the chip and weapon
  records), `robot_mover`, `enemy_path`, `tutorial` (the lesson
  steps and their goals), `training_battle` (the data and the controls),
  `chase_camera`, `robot_library` (animators, stride speeds) and
  `skeleton` (poses with the leg IK). Desktop: `TrainingScreen`
  (`desktop_training.cpp`), `BattleView` (robots and shot effects),
  `BattleHud`, and the world renderer.
- The world fills the window with a Hor+ projection, so a wider window
  shows more. The HUD and messages stay in the 640x480 canvas, the HUD's
  corners following the window's edges.

## The world renderer

`WorldRenderer` (`desktop_world_{renderer,passes,gpu,shaders,shadows,sprites,screen}.cpp`) draws
with Direct3D 11 on the device of SDL's renderer. SDL stays in charge of the
2D.

- It keeps its own `ID3DDeviceContextState`. It flushes SDL's queued
  commands, swaps its state in, draws, and swaps SDL's state back.
- It renders at the window's pixel size into 4x MSAA colour and D24S8 depth
  targets.
- It resolves into an SDL target texture, which is then drawn over the whole
  window. 2D drawn afterwards lands on top.
- Passes, in order:
  1. The sky: the stage's sky nodes, drawn with the view's translation
     removed, then the depth buffer is cleared.
  2. Opaque and alpha-tested draws, except shadow casters.
  3. Shadow volumes, then the casters, then the shadow cover (below).
  4. Alpha-blended and additive draws, sorted back to front by distance.
     Items drawn see-through (a camouflaged robot) join this pass; each
     first writes its own depth with colour writes off, so only its nearest
     surface blends and its inner faces stay hidden.
  5. Sprites: particles, flashes and pickup glows (below).
  6. The screen pass: the stage's tint and depth of field (below).
  7. The cockpit, over a cleared depth buffer.
- Each item carries its node matrices, so a robot draws as its part models
  with every mesh node placed by the pose (`RobotVisual`,
  `desktop_robot.cpp`): the body and legs share the leg skeleton, each arm
  has its own, and the shoulder weapons ride the upper body's node (flag
  0x4). Items also carry an alpha and a white hit flash.
- The HLSL is compiled at run time with `D3DCompile`. Lighting is an
  approximation: vertex colour × material colour × texture ×
  (ambient + sun · N·L), with normal maps on bump batches and linear fog.
  The original's fixed-function and bump shader setup is not reproduced
  exactly.
- Models load with cgltf (`world_model.cpp`) and are mirrored back into the
  game's left-handed space (see the glTF mapping below).
- Textures resolve through the data roots, so a PNG in `mods/` replaces one
  texture.

### Shadows

The original gives robots two kinds of shadow (0x082e60). The look is in
`data/world/training.json` `battle.robot_shadows`.

- **Volumes.** A player's robot casts a stencil shadow volume (view layer
  3, 0x0b7230), unless the screen is split (+0x11074).
  - Robots are drawn after the volumes, with stencil pass ZERO (layers 4
    to 6), so a volume darkens the stage but never a robot.
  - At the end of layer 7 a fullscreen quad (0x0b6b80) lays the stage's
    `shadow_colour` wherever the stencil count is left. The colour comes
    from the table at 0x2014e8 (`stages.json` lighting): 0x70030303 on
    every stage but stage 3.
- **Flat shadows.** Every other robot, while it stands on the ground
  (+0x252c), gets a flat shadow shaped for its legs.
  - The shape is mesh 0x242750[leg type] of resource 184
    (`models/binlink/184.gltf`, `leg_types` `shadow_mesh` in the robot
    catalogue), placed with the robot and coloured 0x60ffffff (view layer 8).
  - Its quads face down, so they are drawn double-sided.

The port builds each part model's volume once (`shadow_volume.cpp`).

- Each solid triangle becomes a two-sided plate, and every edge is closed
  by a quad to its neighbour or to its own other side. Blended and additive
  batches cast nothing.
- The vertex shader moves the corners of faces turned away from the sun by
  `volume_length` (a port value) along the light.
- The original counts with the depth-pass test and repeats a volume when
  the camera is inside it (0x0d2d60). The port counts with depth fail
  (two-sided stencil) and depth clamp instead, which needs closed volumes
  and does not care where the camera is.

Not ported: layer 7's static volumes (VS 0x152), which only draw a marker
cube on the training ground.

City conversion excludes layer 7 from the colour model. Rendering those
volumes as ordinary meshes produced the large grey surfaces over streets
and buildings. Object slots are resolved relative to the model resource's
`first_mesh_slot`, not the smallest slot in the stage object list: Shinjuku
day's resource starts at slot 31 but its object list starts at 91. Its sky
slots 143 and 144 therefore select meshes 112 and 113. The stage table in
`data/world/stages.json` uses those resource-relative sky node names.

City scenery uses a second list, at `0x22e638[stage]`: `0x06f0ab` reads
52-byte records until mesh slot -1. `0x070b80` places the first mesh slot
with XYZ rotation then translation; the second slot is its wreck. The
collision-template index at +44 selects an object kind through `0x22e7e8`.
These are effect models 198-203, bound to texture packs 1575-1580, rather
than the static stage models. `stage_objects.py` converts their intact
instances to `<stage>_props.gltf` (20 per Shinjuku variant, 52 per Shibuya,
25 per Umihotaru), with editable parent transforms and source record
metadata. That baked model remains a fallback for older extractions. The
runtime loads `tables/stage_objects.json` and draws each intact or wreck
mesh from `<stage>_objects.gltf` at the record's transform. Its twelve
collision triangles come from `0x22e658[stage]`, template stride `0x1b0`.
Projectiles test these surfaces before targets behind them; blasts use
distance to the surfaces. Live battles retain destroyed props on reentry.

The hit-handler table at `0x22e8e0` selects destruction, without an HP
threshold: types 0/1/7 select their wreck and effect 14/13/13 respectively;
types 2/3/4 vanish with effect 15 and type 5 with effect 58. Type 0 wrecks
remain solid; type 1 wrecks can be shot away with effect 15 after six
seconds; type 7 wrecks remain visible without shot collision. Type 8 stays
intact. The extracted table exposes effects, collision, wreck meshes,
cooldown and break sound as editable data. These handler interpretations
come from `0x07271e`, `0x072902`, `0x072afa` and `0x07354e`; the prop shot
dispatch is `0x047ed9`.

Type 6 records are gate leaves. They are excluded from that scenery model
and exported through `battle_scenes.py` to the existing `StageDoors`
system: three paired gates per Shinjuku variant and two in each other
city. Each city's `door_model` uses its own textured effect model. The
record and transform interpretation was checked with Ghidra and radare2;
names in this description are researcher-assigned.

### The screen pass

Once a frame, just before the cockpit's view layer 0xb, 0x0b7900 draws the
scene back over itself through one of three passes. Each one multiplies
the frame by the colour at 0x25b490:

- **Plain** (0x0b6f00): the frame times the texture factor. Outside battles
  the colour is white, so nothing changes.
- **Depth of field** (0x0b5fb0), whenever 0x9cb16c is set. Battle setup
  (0x046aa5) sets it and battle teardown (0x047aab) and the scene reset
  0x05741b clear it, so it runs in every battle.
  - `blur.xpu` (program 9, the average of four taps) softens the frame
    twice: into a 320x240 target, then into a second one. The taps sit on
    the corners one texel out (quads 0x25b6a8, 0x25b768).
  - `dof.xpu` (program 8) mixes the soft copy in by
    `saturate((depth - threshold) * 32)`. The depth is the top byte of
    the 24-bit depth, read as the alpha of an ARGB texture. The result is
    multiplied by the colour.
  - The threshold is `focus * 0.0005 + 0.95`. 0x0b2690 sets the focus from
    the table at 0x203820 (80 on every stage), less up to 10 from bits 20-29
    of the player's camera word +0x9cc. That makes the threshold 0.985 to 0.99.
  - Under the battle projection (0x0b7cc0: near 0.1, far 10000) the blur
    starts about 8.5 units out. It steps up at 12.8 and 25.6 units, and
    reaches its most, a third of the soft copy at focus 80, past 25.6.
  - On stage 0 (the byte table at 0x203840) the pass instead uses
    `dofblur.xpu` (program 0x1c) with a bump-env wave. 0x0b2770 rebuilds a
    64x64 offset texture every frame: row i is (sin(i*pi/4 + phase) + 1) / 2
    * (64 - i) / 64 * 50, and the phase steps 0.2 per frame, wrapping at 2*pi.
- **Shimmer** (0x0b6660): the blur, then `shimmer.xpu` (program 0xd) with a
  scrolling texture. It is gated by the byte 0x9c7140, which nothing writes,
  so it never runs.

The colour at 0x25b490 is the stage preset's tint: the four ints at +0x24
(A, R, G, B), packed by 0x0b20a0 in battle setup (0x044b18) and reset to
white at teardown (0x044cee). Fix1 (the training ground) is
(250, 245, 205), a warm cast. Mizuko1 (the ground at night) is
(220, 235, 230).

Nothing else found uses the post programs: `RadBlur` (program 12) is never
set, and `vision_eff` (0x16) is a material program set by the batch draw
0x0b3240 on flag 0x200000.

The port (`desktop_world_screen.cpp`) draws the pass after the sprites,
from `stages.json` `screen`:
- It resolves the frame and blurs it twice into half-size targets.
- The taps are one 640x480 texel (`blur_texels`) out on both axes at any
  window size.
- It turns the port's depth back into distance, then into the original's
  depth byte, so the blur starts and steps where the original's does.
- It writes the mix times the tint into the multisampled target, under the
  cockpit.

Not ported: the focus's per-battle lowering (the meaning of the camera word
+0x9cc is unknown), and stage 0's wave, since stage 0 is not a port stage.

### Sprites and particle effects

Particles and pickup glows are camera-facing sprites (`desktop_world_sprites.cpp`).
They are drawn after the see-through items, depth tested and not written.
The original writes them into one buffer (0x0bf4cf), draws them with
`particle.xvu` and `particle.xpu`, and blends with ONE / INVSRCALPHA:
- `particle.xvu` reads each sprite's constants through a0: position, colour,
  and cell plus rotation. It passes the colour as oD0 and the position's w
  as oT1. The w is 0 for an additive definition and 1 otherwise.
- `particle.xpu` has two combiners. The first gives r0 = v0 · t0. The second
  gives rgb = r0.rgb · r0.a and alpha = r0.a · t1.
- The output is premultiplied. An additive sprite has alpha 0, so it adds;
  any other sprite is laid over what is behind it. The port's shader does
  the same, and fades additive sprites out with the fog.

The effect bank (`tables/effects.json`, converted from the XBE) and the
emitters that run it are described in [EFFECTS.md](EFFECTS.md).

Shots are drawn with their look's model from the weapon effects model
(`desktop_shots.cpp`), a missile's exhaust behind it. Where a shot lands,
its impact group starts. The group flashes (fireballs, smoke, glows and
sparks) are sprites too. A destroyed robot starts the wreck groups and the
supply carrier's crash starts its own. See
[EFFECTS.md](EFFECTS.md#shots-and-impacts).

## The tutorial battle

### Lessons

The robot state table at 0x242d38 holds three handlers per state; states
0x18-0x2e (24-46) are the lessons. Each state shows up to two message
scripts from the table at 0x243168; the table at 0x243220 replaces it when
the robot's flag 0x2740 bit 0 is set. 0x027d5a sets that bit when BUTTON
CONFIG's CONTROL TYPE is Standard (the default), so the second table is
the Standard type's wording and the first the Preferred type's. The
converter writes the Standard scripts under `"standard"` where they differ
(states 25, 26, 31, 45 and 46), and the port picks them by the player's
control type. A course
step plays its first message with the controls locked (0x092070), waits for
its goal, then plays its second message and moves on.

| State | Goal (amount) | Handler |
|---|---|---|
| 24, 35, 42 | Introductions | |
| 25 | Walk forward and back (15 each; the stick is clamped to 0.7) | 0x08b5c3 |
| 26, 27 | Turn on the spot right, then left (2 pi) | 0x08b815, 0x08b9dd |
| 28 | Dash (80) | 0x08bbb7 |
| 29, 30 | Jump; a high jump (above 8) | 0x08bd7d, 0x08befe |
| 31 | Dodge both ways (2 each) | 0x08c0c1 |
| 32, 33 | Twist the upper body past 0.7854 each way, then straighten | 0x08c294, 0x08c43a |
| 36-39 | Three kills each with the handgun, drill, missiles, grenades | 0x08c792 |
| 40 | Ten kills with every weapon | 0x08cdce |
| 43 | Optic Camouflage on, with a drone placed 30 ahead | 0x08d09e, 0x08d151 |
| 34, 41, 44 | End of the course | |

The drones walk only while a step that uses them is practised (0x091ff4).

### Messages

The scripts are the talk scenes' 13-int commands. Pepper's window is sprite
1397 at half alpha with her portrait 823, her name in #66eeee and the text
in #ee9500. The lessons also use opcode 5, a *hold* (0x0c6a0f): it waits a
number of frames with the page still drawn, and once passed it is not
waited for again, unlike opcode 2, which ends the page. Holding the
fast-forward buttons (mask 0xc0c0: triggers and stick clicks) cuts holds
short; the lesson texts are not skippable, so they stay on screen until
confirmed.

### Controls

| Button | Action |
|---|---|
| Left stick | Walk; sideways alone turns on the spot |
| Left stick click | Dash |
| A | Jump; holding it lifts higher for a moment |
| White / black | Dodge left / right |
| Right stick | Twist and pitch the upper body |
| Right stick click | Straighten the upper body |
| Right trigger, left trigger, B, Y | Right arm, left arm, right shoulder, left shoulder weapons |
| X | Optic Camouflage (FreePlay and the optic lesson) |
| Back | Leave the battle |

The buttons are in `training.json` (`battle.buttons`); the keyboard reaches
them through the usual `desktop_input.cpp` mapping.

### Robots, drones and weapons

- Robots face local -Z; a heading of 0 looks along -Z and a positive
  heading change turns right. The root is `rotation_y(heading) *
  translation(position)`.
- The original moves robots by root motion (0x0910d4): the leg skeleton's
  node flagged 0x2 carries the body along the walk. The pose keeps the body
  still and moves that travel onto the foot targets, and the port walks and
  dashes at the speed the leg motions stride (`stride_speeds`), so the feet
  stay planted and each leg part keeps its own pace (the default legs
  stride at about 20 units a second walking and 32 dashing). Legs without
  root motion (the hovering drones) use the `movement` speeds.
- The legs are posed by IK: a one-bone chain to the motion's knee target,
  then a two-bone chain to its foot target.
- Drones start at the path's entry segments (kind 0, at the arena's
  corners), follow its linked segments, and respawn at an entry after being
  destroyed. In the tutorial they do not fire.
- Weapons: guns fire at the lock or along the aim; the drill strikes a lock
  in reach; missiles climb from their start speed to their top speed, then
  home; a record's lobbed round leaves at its launch angle above the aim
  (toward the lock when locked) and falls at 49 units/s² (0x097c30). A trigger
  pull fires the weapon's rounds, the extras scattered within its spread.
  Projectiles expire after their lifetime. A record's blast weapon hurts
  the body it hits, then its blast grows from its start radius to its full
  one, hurting every enemy inside it each tick. A homing missile's rounds
  leave one every 10 frames. Hits follow the damage rules (0x0988e1):
  triple damage from behind, dodges, criticals that can knock off a weapon,
  stun and knockback ([PART_RECORDS.md](PART_RECORDS.md)). Each weapon picks
  the enemy nearest its aim inside its cone and range, seeking from the aim
  origin; a camouflaged robot cannot be picked. Guns and the drill use it
  at once. Missiles and grenades lock only once it has been held for the AI
  chip's lock time, and fire unguided before that.
- Parts: the player's AI chip and weapons name original records in
  `tables/parts.json`, which the robot converter reads from the executable
  (`parts.py`). The chip (0x1b7fc8, copied to robot +0x4504 by 0x092c6c)
  gives the reticles' shapes and sizes, the tracking speed and the lock
  time. A weapon record (arm 0x1bae48, shoulder 0x1be9a8) gives the HUD
  kind, damage, fire interval, rounds, spread, speeds, lifetime, homing,
  launch angle, blast, ammunition, beam recharge, stun, knockback and
  knockdown ([PART_RECORDS.md](PART_RECORDS.md)). The chip and legs give
  the robot its critical, dodge and resistances.
  The robots' armour is their body and legs records' (0x076114), each raised
  to its floor. Values grow with the part's level along a curve (0x092160,
  0x0924fd, 0x092311).
  The new-game robot was not found, so the port chooses Dog I with
  Pistol/HM15, A22c7 Drill Arm, M-24L/1 Aspide and G24 Slingshot, all at
  level 0.
- Ammunition: each round spends one, but a machine gun spends one a pull
  (0x09b4ed); melee weapons need none. A refill
  counts the shown amount up over 120 frames (0x09b986). Weapons refill
  only from supply pickups (0x06ad29, see [Supply drops](#supply-drops)).
  `ammo_refill_seconds` above 0 is an opt-in port refill of an empty
  weapon on a timer; it is 0, off, by default.
- Camouflage turns the robot see-through (the world renderer's per-item
  alpha) while its gauge lasts; hits flash the robot white.
- Robots collide with the stage as two spheres of their leg type's radius
  (0x07c7c0, see [Stage collision](#stage-collision)) and stand on its floor.
  Without a collision file the battle falls back to `battle.arena`, the
  square |x|, |z| <= 100 less the four corner towers. Shots stop at the
  first wall or floor they cross.
- Gravity is 9.8 units/s² times the world's 2.5 (0x07d117 at the battle's
  30 Hz, 0x046aa5). The upper-body twist turns 3 degrees a tick, within 45
  degrees plus 22.5 times the smoothed stick, and its reset eases back over
  8 ticks (0x07d330).

The weapons' lock cones, reach and stagger, and the other movement, camera
and HUD values, are port values until the robot stats and the battle camera
are decoded; all of them are in `training.json`.

### The HUD

The HUD pass 0x4e6b8 (state 0x984 bytes, set up by 0x4e318) runs per robot.
The camera view (robot +0x118: 0 first person, 1 behind, 2-5 orbit) decides
what it draws:
- In first person, 0x4c459 draws 3D cockpit models.
- In the other views, 0x4aaed draws the DURABILITY and O.C TIME gauges.
- It always calls 0x4ad3b for the weapon panels and the rest of the 2D
  panels.
- In views 0 and 1 (the port's chase camera is view 1) it also calls the
  world drawers below.

Projected points go into the 576x448 safe area, so offsets about a
projected point are scaled 0.9 and 0.93. The port keeps those scales but puts
the points where the world is drawn. `data/ui/battle_hud.json` lays out the
world pieces from `game_tex` (slot 464, sprites 1354-1396). The panels come
from `data/ui/battle_panels.json`, using `game_quest_tex` (slot 465, sprites
1397-1434). Split screen uses `game_vs_tex` and other records; see
[Split screen](#split-screen).

| Drawer | Pieces |
| --- | --- |
| 0x503b9 | The sight ring (1392) at the projection of the point 64 units along the robot's aim. Four triangles 55 units out (0x203d08); they fill (1393) with a pulsing red while a camouflaged robot is on screen within 75 units. Arrows 96 units out toward robots within 75 units that are off screen and less than 90 degrees from the heading. |
| 0x4faad / 0x51499 | The inner and outer reticles and the centre diamond (1387). The half size comes from the shape tables 0x203c48 and 0x203c78 (square, wide, tall), interpolated by the fire control's value / 999 (0x4db69). Each corner piece is the top-left one, mirrored and turned for the others. |
| 0x4f6a0 | The reticle's tracking: it follows the nearest enemy inside the inner box at (0.02 + 0.08 × robot +0x4514 / 999) NDC per frame. The port checks acquisition against the sight-centred chip box and bounds the tracked offset by its half width/height, preventing the moving reticle from dragging the acquisition area beyond the chip's limits. |
| 0x51045 / 0x51499 | Shoulder lock boxes (1390 right, 1389 left, colour 0xcc00b496). Each opens at the outer reticle's corners and closes on the robot as the lock time (robot +0x4520) runs out, keeping (time left / lock time) of its spread each frame (0x5239b). |
| 0x51e37 / 0x5218b | The melee lock box (1391) for a kind 6 arm weapon, within 20 units (25 for other robots). |
| 0x4de50 | The markers: a team-coloured frame (1376) whose tail points at the robot, and its armour bar (1386) scaled by armour / maximum. The robot must be on screen, uncamouflaged and within 75 units. |
| 0x4aaed | DURABILITY (1399, fill 1398 over 128) and O.C TIME (1401, fill 1400 over 72), with low-value colours and the colour flash 0x4dc2f. |
| 0x4ad3b | The four weapon panels from the records at 0x204340 (left) and 0x2046b0 (right). Each has a frame (1430/1431), the icon for the weapon's kind, an ammunition count or charge percentage with a bar, and WEAPON LOST. It also draws DAY-, TIME, MONEY and SHOOTDOWN with their numbers, and the DAMAGED / HIT / BINGO! messages rising at 48,388. Below half armour it shows SYS DURABILITY DOWN 50% / 75%, and MISSILE ALERT while a missile homes on the robot. |

The fire control comes from the player's AI chip: Dog I has a square inner
reticle, a tall outer one, a slow track (3 of 999) and a 22-frame lock.
Each lock box closes over the lock time while the battle holds the target,
and a missile or grenade locks when it has closed. The weapon panels show
the icon for the record's kind (the homing missile icon when +0x40 is not
zero) and its readout: the ammunition for kinds 0-3, a charge for kind 5,
nothing for kind 6. A panel flashes while its ammunition refills.

The messages follow the original: HIT goes to a robot whose hit damaged
another (0x09d2ce). When a robot is shot down (0x07675e), it gets DAMAGED if
others had damaged it, and every robot that damaged it scores a kill and
gets BINGO!. DAY-, TIME, MONEY and SHOOTDOWN are placed from their labels'
left edges and widths and the font's advance (0xd381c). TIME is the battle
clock as HH:MM:SS.

Where the port departs from the original:
- **Loadout.** The player's chip and weapons are the port's choice. The
  robots have no option parts, so their armour, fight stats and weight are
  their parts' alone. The load scales the port's turn rates and jump
  rather than setting the original's own values.
- **Hits.** A stunned robot plays the hit reaction for the push's
  direction for the weapon's `stagger_seconds`, at least the motion's 29
  frames.
- **Chatter.** The player's robot speaks from its chip's line table
  (Dog I: the report voice). Every event the original raises is raised
  (the list is in [PART_RECORDS.md](PART_RECORDS.md)) except 31, a
  breakable stage object shot, since the port has no breakable objects.
  The drones have no pilot and say nothing. The drones never fire, so the
  lines about being hit are rarely heard.
- **Name tags.** The markers draw a pilot's tag, but the training robots have
  no pilot (`pilot` in `training.json` gives one).
- **Supply drops.** Only FreePlay has them. The computer robots do not go
  for the pickups (0x0678f8 is not ported), and the carrier has no engine
  sound yet.
- **Goal bar.** The lesson goal bar in the top left is the port's own; the
  original leaves the goals to Pepper's messages.
- **Widescreen.** With `"layout": "widescreen"`, the left and right panels
  keep to the window's edges.

### Camera views

The camera views (camera +0, the placement table at 0x23289c run by
0x07fea8) are in `training.json` `camera_views`. The D-pad changes them
(0x0774ad) while the robot is free to move.

| View | Drawer | Camera |
| --- | --- | --- |
| 0 first person | 0x07e901 | In the cockpit: the eye a legs-model height above and ahead of the torso (0x2327f0, 0x232848), looking 24 units ahead. |
| 1 behind | 0x07edfa | The chase camera, the start view (save +0x6048). |
| 2 right side | 0x07eb61 | (-5.5, 1.65, 0) from the feet in the camera's yaw frame. |
| 3 left side | 0x07ec2f | (8, 5, 0), rising 2 per radian of aim pitch (0x2327c4). |
| 4 front | 0x07ed12 | (0, 1.65, -8), looking back at the robot, rising with the pitch. |
| 5 fixed | 0x07f641 | The stage's fixed points (0x232ae0): every 30 ticks, the nearest of the three nearest points that sees the robot. |

Each view has its own field of view (camera +0x14 + view × 4); all six are
52 degrees in the original (0x07e10c). First person and behind blend over
20 ticks, and the other views cut. The yaw and pitch ease 0.3 a tick
(0x07fc5e, 0x07fd72). The HUD's world pieces are drawn only in views 0 and
1 (0x04eaa3).

### Cockpit

In first person, 0x04c459 draws the cockpit model (binlink 216) in the eye's
space, 8 units ahead, instead of the DURABILITY and O.C TIME gauges. It
comes up over the second half of the change to first person and slides out
over the first half of the change away. The robot is hidden once the view is
nine tenths first person (a port threshold). `data/ui/cockpit.json` holds
its pieces:
- The armour and camouflage gauges, whose UVs step through cells of the
  cockpit texture (0x205620, 0x205708, 0x205698, 0x2056d0).
- The speed and climb gauges. The climb gauge is full at the speed gravity
  gives over 6 ticks falling or 2.5 rising.
- Pieces that bob with the robot and sway with the twist.
- While camouflaged, the visor tint: mesh 0 scaled 1.5 wide, one unit
  ahead, added in 0x1e0f64 at the camouflage's strength.

### Optic Camouflage

The look comes from the disc's shaders (`Media/Shaders/optical.xvu`,
`multi_optical.xvu`, `Blend.xpu`) and their setup (0x0c05c2, 0x0c21db). The
values are in `training.json` `optical_camouflage`.
- **Sphere.** Every robot vertex is measured against a sphere. Inside it the
  robot is seen as usual.
- **Refraction.** Outside the sphere the robot shows the frame behind it,
  sampled where the vertex would land pushed out along its normal, so the
  background bends around the robot's shape. The world renderer copies the
  frame before it draws the camouflaged robots and samples that copy.
- **Fade.** While the camouflage turns on, the sphere's centre rises from
  the robot's base (0x074e80, the table at 0xa94640), so the robot vanishes
  from the bottom up. Turning it off lowers the sphere again.

### Supply drops

FreePlay's supply drops follow `training.json` `supply` and the stage's
routes in `tables/battle_scenes.json`:
- **Carrier.** It flies a route of Bezier segments. It opens its hatch on
  the closing segment and drops its pickups one after another on the drop
  segment. When every pickup is gone, it starts again.
- **Model.** Even stages use model 207 and odd stages model 208 (0x0699d5).
  Both have the body, a wreck, two hinged doors, a spinning part and the
  pickup crates. Odd stages add a lights shell drawn in a random colour
  every frame.
- **Pickups.** A pickup falls, lands, blinks as its lifetime runs out
  (0x066e50) and is taken by a robot touching it. Its kind comes from the
  chances table: repair, ammunition or money (100 to the player).
- **Glow.** Each pickup carries a glow (0x069d52): the grid_mdl quad with
  texture 1574, 3.7 across, pulled 1.8 toward the camera and blended
  normally at |sin(blink)| × 0x60/255. Money glows 0x7070d0, repair 0xa0a0a0
  and ammunition 0x70d070 (`look.glow_*`). In split screen the crate is
  tinted instead, by its colour × (|sin| × 0.8 + 0.7).
- **Taking.** A take throws effect group 0x31, 0x32 or 0x33 (money, repair,
  ammunition) at the pickup, aimed at the robot's lower sphere
  (0x087eac, 0x0880ad). The rate is 2400 times the kind's level within its
  effect, so the burst throws its 111 sparks in one frame and stops. The
  sparks home on the robot as it moves. At most three bursts run at once
  (`look.take_*`).
- Not ported: the original swaps the crate between the 207 and 208 meshes
  at random every frame.
- **Shooting.** The carrier (radius 3) and falling or landed pickups
  (radius 1) take shots (0x067603). A shot pickup bursts. A carrier shot
  down pays its shooter 1000 (0x047ed9) and immediately starts explosion
  group 0x3f (0x06781a). The wreck keeps 0.6 of its horizontal speed and
  sinks 0.3 units per 30 Hz tick; it does not keep following its route.
  After 500 ticks, or on contacting the stage (including walls), it starts
  group 0x3f again and disappears (0x06828e). Cargo still carried is removed
  at the initial hit. Already falling and landed pickups remain available;
  neither visual explosion damages them. A fresh trip begins once no
  released pickup remains. `supply_destruction_tests.cpp` covers immediate
  effects, drift/fall, cargo separation, contact and timeout.
- **Cadence check.** The original battle clock table at `0x255ab0` sets
  scenes 10 and 42 to 30 Hz. Initial departure (`0x066940`) and subsequent
  trips (`0x06a2f6`) pass `0x3cf5c28f` (0.03) to `0x0679e7`; all three
  flight states add that step to the cubic route parameter each update.
  The port's fixed 60 Hz host passes 1/60 second, so `SupplyDrop` advances
  0.015 per host tick, or 0.9 route segments per second. This is parameter
  speed, not constant world-space speed: longer curve sections cover more
  distance in the same time. The monitor pass never advances simulation,
  and `RumblingWorld` skips ticking the player's borrowed battle.
  `supply_drop_tests.cpp` checks ten seconds of travel at 30/60/120/144/480
  presentation Hz and irregular frame timing against 30 Hz simulation.

### Split screen

With more than one screen (`FUN_000ee540 + DAT_00416998` counts them), the
window is divided. Two screens are the top and bottom halves, 640x240 each.
Three or four are the quarters, 320x240 each. Each screen draws the world
from its robot's camera and its own HUD, in coordinates local to that screen:
- **Panels.** They come from `game_vs_tex` (slot 466):
  - `data/ui/battle_panels_split2.json`: the gauges 0x204148 and the weapon
    records 0x204a20 (left) and 0x205100 (right).
  - `data/ui/battle_panels_split4.json`: the gauges 0x203f50 (the two-screen
    positions, 150 to the left) and the weapon records 0x204a20 and
    0x204d90.
- **Weapon panels.** Each is a 64x16 icon strip on its frame, with the
  digits above it.
- **Left out.** With more than one screen, the ammunition and charge bars,
  the DAY, TIME, MONEY and SHOOTDOWN labels and the DAMAGED / HIT / BINGO!
  messages are not drawn (0x4aaed, 0x4ad3b).
- **Sights.** 0x4faad, 0x503b9, 0x51499 and 0x5218b draw their pieces at half
  size. Their offsets are fractions of the screen, so they shrink with it.
  The markers (0x4de50) keep their size.

The split screen is VS mode's (`vs_battle`, one screen for each player).
Tools > Level Select opens `training_split2` and `training_split4` to test
the layouts: FreePlay with two or four screens, following the player's
robot and then the drones.

### Entrance and exit

A robot's state (robot +0x276c, the state table at 0x242d38, run by
0x088b69) gives FreePlay a scripted entrance and exit. Robot creation
(0x080778) starts a FreePlay robot in state 1 (0x082552, save +0x10f10 3)
at its gate. The stage's gate table (0x201598) has four gates on the
training ground, inside the corner towers, and the player takes gate
robot index % 4 (0x0825df). The lessons start in their own states (0x18,
0x23, 0x2a), where the script puts the robot.

- **Entrance** (states 1-4: 0x088ffa, 0x08928c, 0x0897a2). The player's
  robot picks one of the four states at random; state 5 is a boss's.
  Each is 540 60 Hz frames with its own camera (the scene table
  at 0x258f50: roll, field of view, eye and target channels in the motion
  bank's format, placed at the gate and turned by its heading) and the
  same walk, 15 units forward between frames 119 and 359, turned by the
  gate's heading. The leg motion strides with the distance covered, and
  the robot stands with 180 frames left. The letterbox bars are in from
  the start (0x049080). READY (sprite 0xe80) shows with 90 frames left and
  GO! (0xe7f) with 30, each with its sys sound (entries 0 and 1). Over the
  last 20 frames the camera eases into the chase view (0x0800e8). At the
  end the player takes the robot, the bars slide out and the HUD (hidden
  in these states, 0x04eaa3) returns. Until then the controls do nothing
  (0x082e60). In regular battles (modes 0 and 2) the music cue, track 16
  9PLAYIN, starts 6 battle ticks (12 frames) before the end; the
  tutorial's mode 4 has none.
- **Computer robots** (state 1: 0x082552, 0x088ffa, 0x08928c, 0x0897a2).
  Computer robots start at gate robot index % 4 with a delay of 120
  frames (+0x277c). While the gate's door is busy they wait, until the
  delay runs out. In regular battles they also wait for the player's
  scene and for a boss entering (0x09199e, which answers no in mode 4).
  Their entrance plays three times as fast (0x17cce0) and ends when it
  reaches 180 frames left; the robot then takes up the enemy path. In the
  port a drone that respawns enters again.
- **Boss** (state 5, 0x082cff: a rival whose loadout +0x70 is 1). The
  battle's first boss (global +0xef328) plays its entrance at full speed.
  The bars slide in and the camera takes state 5's shot, placed at the
  boss's gate, with no easing (0x07e456). Track 6, 13BossStart, plays.
  When it stands (0x0897a2) the bars slide out. The other robots fight on
  through the scene: 0x091dc4 sets their tasks' flags 0x24, but in the
  running game they keep moving and shooting, so the port does not hold
  them (it only keeps the player's controls and the HUD waiting, and other
  computer robots at their gates). A drone with `"boss": true` in
  `training.json` gets this entrance.
- **Entry shield** (robot +0x27d4). A computer robot is shielded for 300
  frames from its entrance (0x082552), renewed while it waits at its gate
  (0x089032, 0x0892d1). The player's shield is raised immediately on
  placement and refreshed throughout its entrance (0x08928c writes 300
  frames at 0x0892d1), then counts down for five seconds after control
  returns. The player's shield visual stays hidden throughout the elevator
  entrance and appears when control returns; damage protection remains active
  throughout, including while the visual is hidden. Arena monitors use the same
  visibility rule. The player's exit is protected for its scene (0x08224d).
  No hit hurts a shielded robot. Its glow (+0xac, 0x082e60) rises 0.015 a
  frame to 0.3 while more than 10 frames are left, then falls 0.03 a
  frame. 0x06f592 draws it: object slot 0x41b (mesh 30 of resource 197, a
  sphere one unit across) at the lower collision sphere's centre, 3.2 leg
  radii across, turning 0.02 a frame, colour 0x64ffffff times the glow,
  added to what is behind it. The original uses `rainbow.xvu` / `rainbow.xpu`:
  animated height-dependent vertex colour and scrolling alpha waves from
  `systemdata/muteki01.tga`. The current port's white textured sphere
  (`models/binlink/197_30.gltf`) omits that shader and applies an extra alpha
  multiplier. See [Entry shield rendering](ENTRY_SHIELD_RENDERING.md) for the
  validated shader dispatch, constants and native discrepancies.
- **Doors** (stage objects of type 6: 0x072f85, 0x0731c5, 0x073430). Each
  gate has a door of two curved leaves, choco_effmodel meshes 4 and 5.
  The leaves turn 0.58 radians about the tower's axis, opposite ways,
  over 30 battle ticks. After 20 ticks robots pass; the door holds open
  until 150 ticks after it began, then closes over 30. Opening plays
  battle_player sounds 0x31 and 0x32, closing 0x31. An entrance opens its
  gate's door as it passes 480 frames left (0x070667). A shut leaf is a
  wall of 12 triangles (0x06f82e), and a robot that walks into one opens
  the door, or turns a closing one back. That is how the exit gets in.
  With 30 frames of the exit left, 0x0707ae shuts a fully open door and
  locks it (flag 0x1000) until it has closed. Shots land on a shut leaf as
  on any wall. A scripted walk goes
  through the leaves, because 0x08928c sets the robot's position outright.
- **Exit** (state 6: 0x089fa3, 0x08a161, 0x08a496). Once the robot has
  been outside every gate, walking back into one (0x08224d; 0x044839: up
  to 10.5 above the gate point and within 5.97 across) starts the exit if
  it still has armour. The chip signs off (chatter 0x1c, "hit and run", or
  9 below a quarter of its armour) and the bars slide in. The robot turns
  toward the gate 6 degrees a battle tick and walks into it at up to 0.2
  units a tick. After 240 frames the battle ends (scene 42 sets battle
  mode 6).
- **Computer robots' exits.** 0x08224d runs for every robot, so a computer
  robot that walks back into a gate leaves too (not in the story's
  missions of types 0x16 and 0x17, 0x082c54). Its exit (0x089fa3) has no
  bars or camera, and it does not shut the door. With the walk done,
  0x08a496 keeps it standing in the gate, shielded for 30 frames at a
  time; once the gate's door has shut (0x070b1f) it counts as gone
  (+0x4c 120), and the roster loop 0x047c9e takes it out of the battle as
  it does a robot shot down. In the port a drone then respawns
  (`computer_exit`, `gone_shield_frames`).
- **Chatter.** While the player's scene runs, only robot 0's lines 0x12,
  9 and 0x1c play ([PART_RECORDS.md](PART_RECORDS.md)). The entrances
  raise their lines only in battle modes 0 and 2, not the tutorial's
  mode 4: 0x12 for the player and allies, 0 for a rival, 4 for a boss. So
  FreePlay enters quietly. `regular_battle` switches the mode 0 and 2
  rules on (the lines, the music cue and the computer robots' waits).

The converter writes the cameras, the walk, the gates and the gate doors
(the stage object records at 0x22e638, their types at 0x22e7e8 and
collision templates at 0x22e658) to `tables/battle_scenes.json`
(`battle_scenes.py`). The timing and the rules
are `battle.scenes` in `training.json`; the bars and the READY and GO!
sprites are in `data/ui/battle_scene.json`. In the port, `BattleScene`
(`port/src/battle_scene.cpp`) walks the robot with `RobotMover`'s scripted
mode and gives `ChaseCamera` its shot, and `BattleScenePresenter` draws the
bars across the whole window. Computer entrances and the boss's are in
`battle_scene_robots.cpp`. The doors are `StageDoors`
(`port/src/stage_doors.cpp`), which `Battle` ticks and pushes robots
against. The screen draws each leaf as one node of the effect model. The
jingles are the screen's music until they end.

Port choices: the gate test takes the robot's lower collision sphere's
centre (the point the original tests, robot +0x2790, is not identified);
the field of view eases with the eye and target; the exit turns toward the
gate point. A held robot keeps its pose or gets no controls. The chase
camera's eye stays in front of a shut door leaf. The door sounds play at
their door ([BATTLE_SOUNDS.md](BATTLE_SOUNDS.md)). A stage in
`data/world/stages.json` names its number in the original's tables
(`scene_stage`) and its doors' model (`door_model`): the night training
ground, stage 7, has the same gates and doors drawn from
choco_night_effmodel. Not ported: the story missions' gates and waits
(mission types 0x16 and 0x17).

### Performance

`--benchmark [seconds]` runs without vsync or the frame cap, leaves out
its first 2 seconds and prints the frame times and each frame's work before
it is presented, with the share under 1.5 frames of 144 fps (10.42 ms).
`--press` works with it, by logic tick, to drive a battle. A screen loads
the images, sounds and models it will use as it opens
(`TextureCache::preload` on its data documents, `Screen::sound_preloads`),
so nothing loads mid-battle.

On the development machine (a 144 Hz display, 4x MSAA; a windowed present
waits for the display's refresh, so frames sit at 6.94 ms):

| Scene | Size | fps | On time | Work p99 |
|---|---|---|---|---|
| FreePlay, firing | 1920x1080 windowed | 144.0 | 99.90% | 0.35 ms |
| VS, two screens, both firing | 1920x1080 windowed | 143.4 | 99.95% | 0.38 ms |
| VS, four screens, all firing | 1920x1080 windowed | 143.5 | 99.97% | 0.48 ms |
| VS, four screens, all firing | 2560x1600 fullscreen | 142.2 | 99.73% | 0.46 ms |

The late frames in the VS runs are the frame the battle loads in (about
150 ms), as the entry screen gives way to it.

## The stage

The stage table is at 0x2212ac, and the stage object lists are at 0x2212ec.
The training ground is stage 6, `choco_mdl`.

Its resource record (the resource table is at 0x285E40, with 24-byte
records) gives:

- first model slot: 900
- mesh count: 7
- texture resource: 1564 (`texture/choco_tex`)

Slots 900–906 hold meshes 0–6, with view layers 7, 1, 2 and then 8 for the
last four. Layer 1 is the sky, which is drawn around the camera.

| Mesh | Contents |
|---|---|
| 0 | A small marker cube at (100, 0.5, -108) |
| 1 | The sky dome: radius about 2060, unlit, 5 batches (clouds are textures 0–1, the horizon is 2) |
| 2 | The arena: 6361 vertices and 3604 triangles. Gravel floor (texture 3, repeating), walls (texture 4 with its normal map 5), the emblem (texture 6, texture alpha), towers (texture 7 with normal map 8) |
| 3–6 | Unlit corner quads (texture 9) |

### Cloud motion

The fourth word of each stage object entry at `0x2212ec` is its animation
tag. `0x06e5b8` applies it through `0x06ea05` before submitting the object.
Tags 1 and 2 rotate around Y by minus the shared sky angle, or twice that
angle. The angle advances by `0.0005` per original battle update. Tags 3 and
4 advance texture V by `0.0005` and `0.001` respectively. At the battle's
30 Hz cadence these are −0.015/−0.03 radians and 0.015/0.03 UV units per
second.

`data/world/stages.json` records the corresponding sky nodes and rates in
`sky_motion`. The renderer applies them per node, using elapsed game time;
split screens and arena monitor renders sample the same animation time.
Static sky layers and non-sky surfaces retain their own transforms and UVs.
The daytime training dome has tag 0 and remains stationary; its night sky
has a rotating layer. All six city skies have tagged moving layers.

### Stage collision

The stage's collision is a separate resource, from the table at 0x2212cc
(`[379, 379, 380, 380, 381, 381, 382, 382]`). Stages 6 and 7 use 382,
`model/choco_col.bin`, and the others use resources in binlink packs 4-6.
The stage loader (0x06e040) reads it and 0x052b90 fixes its pointers up.
`collision.py` reads and writes the file, and `convert-robots` writes
`collision/<name>.json` (`{grid, walls, floor}`, triangles as three game
space points and a surface).

| Offset | Contents |
|---|---|
| 0x00 | u32 cells x, y, z (32, 4, 32); f32 cell size x, y, z; u32 wall triangle count; u32 wall index count |
| 0x20 | Wall cells, [x][y][z]: the first index of each cell's list |
| 0x4020 | Two pointers the loader fills |
| 0x4028 | u32 floor triangle count, floor index count |
| 0x4030 | Floor cells, [x][z] |
| 0x5030 | Two pointers the loader fills |
| 0x5038 | The grid's centre |
| 0x5044 | Wall triangles, wall indices, floor triangles, floor indices |

A triangle is 0x34 bytes: three corners, the normal
(`normalize(cross(b - a, c - a))`) and a u32 surface. Each cell's index list
ends with -1. A point's cell is `floor((p - centre) / size + cells / 2)`, and
cells list the triangles whose bounds, grown by about 2.4, touch them.
`choco_col` has 225 wall and 106 floor triangles within ±106.6.

The sphere test (0x052c1f) tries a cell's faces (0x054c45), then edges
(0x05589d), then corners (0x055b68). It returns flags: 1 touched, 8 ground
(the push's normal has y above 0.75), 0x10 wall, and the surface's own flag
(surface % 1000 indexes 0x2057d8; surface 8, the fence, is 0x2000 and starts
the 30-frame contact effect at 0x07027b). The floor query (0x053337) returns
the ground height or -1000. The robot's collision (0x07c7c0) pushes a sphere
of its radius at its feet and another at its head, snaps it to the floor,
and undoes the move if the swept test (0x0539ce) hits. The radius is the leg
type's (`{f32, f32 radius}` at 0x22f420, read at 0x080e41 and clamped to
0..2).

The port (`stage_collision.cpp`) grids the triangles over x and z. It keeps
the original's flags and ground rule, pushes spheres out of faces from
their front only, and tests shots as segments.

### The stage's environment

0x044e9f sets the battle light and 0x0a1590 sets the fog from one 0x54-byte
preset in the list the stage's pointer at 0x2014c8 names. The index into
that list (0x41699c) stays 0 in every path found, so each stage uses its
first preset. The names are the shipped data's own. `environment.py` reads
them all into `tables/environment.json`.

| Offset | Contents |
|---|---|
| +0x00 | Name ("Fix1", "Default1", ...) |
| +0x04 | Fog colour (D3DCOLOR) |
| +0x08 | Fog density, start, end |
| +0x14 | Fog mode: 0 none, 1 linear, 2 exp, 3 exp2 |
| +0x18 | Light colour |
| +0x1c | Diffuse scale, ambient scale |
| +0x24 | Tint: 4 ints (A, R, G, B) packed into 0x25b490, the screen pass's colour multiply |
| +0x34 | Levels (minimum rgb, maximum rgb) for the gamma ramp (0x044974) |
| +0x4c | A flag, and a value stored to 0x416930 |

The light is directional along the stage's vector at 0x2015f8 (the
training ground: (10.52, -16.43, 4.42)). Its diffuse is the light colour
times the diffuse scale, and its ambient the colour times the ambient
scale. The training ground's first preset, Fix1, has white light at 1.04
and 0.23, and linear fog from 0 to 350 in #8c7d70. `stages.json` carries
those values. (The table at 0x22e928, which 0x0736a0 runs after loading a
stage, sets the I3DL2 reverb, not the light.)

## The model format (`*_mdl.bin` and binlink model resources)

`src/phantomcrash_research/model.py` parses this format. All 16 standalone
models and all 210 binlink model resources parse exactly to their end.

### File

The file starts with a magic (0x80000101 or 0x80000102), then the mesh
count, then the meshes.

### Mesh

Each mesh has a 14-word header:

| Word | Meaning |
|---|---|
| 0 | Flags |
| 1 | Vertex count |
| 2 | FVF (0x152, or -1) |
| 3 | Vertex count again |
| 4 | Triangle count |
| 5 | Batch count |
| 6 | Node count |
| 7–9 | Bounds centre |
| 10–12 | Bounds extent |
| 13 | A word not identified yet |

After the header come:

1. A D3D vertex buffer header (3 words).
2. A D3D index buffer header (3 words), only when the mesh has triangles.
3. The vertices, 36 bytes each: position, normal, D3DCOLOR, UV.
4. The u16 indices: triangle lists, indexed within the mesh. With magic
   0x80000102 the triangle count is padded to a multiple of 4.
5. The batch records, 92 bytes each.
6. The node records, 92 bytes each.

### Batch record

| Word | Meaning |
|---|---|
| 0 | Batch flags (below) |
| 1–4 | Flags for texture stages 0–3 (below) |
| 5–8 | Texture indices into the model's texture pack (-1: none) |
| 10 | Material diffuse (ARGB) |
| 11 | Vertex count |
| 12 | Triangle count |
| 13 | First index |
| 14 | First vertex |
| 15 | First triangle |
| 16 | Specular power (50) |
| 17–22 | Bounds |

Batch flags (word 0):

| Flag | Meaning |
|---|---|
| 0x2 | Indexed list, drawn from index `first_triangle × 3` |
| 0x4 | Non-indexed list |
| 0x40 | Specular |
| 0x100 | Unlit |
| 0x200 | Additive (SRCALPHA, ONE) |
| 0x800 | No culling |
| 0x4000 | Blending off, alpha test at 0x80 |

Stage flags (words 1–4):

| Flag | Meaning |
|---|---|
| 0x1 | Colour ADD |
| 0x2 | Bump: when both textures are set, the second texture is the normal map (shader program 2) |
| 0x4 / 0x8 | Mirror U / V |
| 0x10 / 0x20 | Wrap U / V (clamp otherwise) |
| 0x40 | Alpha comes from the texture |

The default cull mode is counter-clockwise (0x901), so clockwise faces are
the front faces.

### Node record

| Word | Meaning |
|---|---|
| 1 | Batch count |
| 2 | Batches drawn in pass 0 |
| 3 | Batches drawn in pass 1 |
| 4 | First batch |
| 5 | Parent |
| 6 | Child |
| 7 | Sibling |
| 8–10 | Translation |
| 11–13 | Rotation (radians) |
| 14–16 | Scale |

The local transform is S·Rx·Ry·Rz·T with row vectors.

### glTF mapping (`gltf.py`)

- Z is negated on positions, normals, tangents and node transforms. This also
  turns the game's clockwise front faces into glTF's counter-clockwise ones.
- Each node's rotation becomes the quaternion qz(z)·qy(−y)·qx(−x).
- Each batch becomes one primitive of its node's mesh.
- Materials:
  - Texture 0 is the base colour; on bump batches texture 1 is the normal
    map.
  - Flag 0x4000 becomes alpha mode MASK.
  - Texture alpha or additive becomes alpha mode BLEND.
  - Unlit batches use `KHR_materials_unlit`.
- `extras.phantomcrash` keeps the original values: flags, stage flags,
  texture indices, the diffuse colour, the specular power, and each
  primitive's pass.
- The converter writes `models/model/<name>.gltf` for every model resource
  that has a disc path.
- Binlink models (the robots, city stages and enemies) need their texture
  banks (`z_robotex_*`) mapped first.

## Next steps

1. The robot stats: the turn rates, the jump, the per-robot aim rate
   (+0x7c, which 0x07d330 multiplies by the stick for the pitch), now known
   as the legs' curves under the load (0x07c34e,
   [PART_RECORDS.md](PART_RECORDS.md)) but not yet read in their units.
2. VS mode, which will use the split screen; the computer robots seeking
   supply pickups (0x0678f8); breakable stage objects (chatter event 31).
3. Robot-contact fence effects (shot impacts now use the extracted
   refraction meshes; see [EFFECTS.md](EFFECTS.md)); specular; the presets'
   gamma levels.
4. The Standard and Preferred control types' movement differences (the
   robot's flag 0x2740 bit 0 in 0x07d330, 0x08b70b and 0x08c18a).
5. The other stages in `stages.json`, from the stage table, with their
   collision and environment presets.
