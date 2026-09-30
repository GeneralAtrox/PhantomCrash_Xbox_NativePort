# The particle effects

The game's particle effects live in the XBE's data, not on the disc. The
converter (`effect_bank.py`, run by `convert-robots`) writes them to
`tables/effects.json`. It also writes the atlas `textures/binlink/1589.png`
and the pickup glow `textures/binlink/1574.png`. The port reads the bank in
`effect_bank.cpp` and runs it in `effect_system.cpp` and
`effect_particles.cpp`. The drawing is described in
[TRAINING_GROUND.md](TRAINING_GROUND.md#sprites-and-particle-effects).

Names in this file are researcher-assigned from what the code does with
each field, unless an address is quoted. Fields named `unknown_*` are read
but their use is not yet known.

## The bank

| Key | Source | Meaning |
| --- | --- | --- |
| `frame_rate` | 30 | Effects step whole frames at the battle's logic rate. |
| `atlas` | constructor 0x11d060 | Texture, cell size (0.125 × 0.25) and the 22 cells' corners in the 8 × 4 grid. |
| `groups` | 0x205c10 | 79 groups. Each is a list of scripts and a list of flashes. |
| `scripts` | 0x215928 | 62 scripts. Each is a list of emitters: flags and a definition index. |
| `definitions` | 0x20d720–0x215cf8 | 71 definitions of 0x150 bytes each. |

Game code starts an effect by its group number (0x057760). Every script in
the group starts one emitter for each of its entries, and each of the
group's flashes starts one flash (0x0659e5, below).

### Emitter flags

The script entry's flags (emitter +0x10) choose how that emitter's
particles behave. `spawn` (0x2), `replaceable` (0x4), `inherit_velocity`
(0x8), `exact_spread` (0x10), `radial_push` (0x20), `random_direction`
(0x40), `spin` (0x80), `streak` (0x100), `streak_long` (0x200),
`streak_short` (0x400), `random_offset` (0x800), `fragments` (0x1000),
`detached` (0x2000), `random_speed` (0x4000), `swirl` (0x8000),
`die_at_target` (0x10000), `debris` (0x20000), `debris_kind` (0x40000),
`kept` (0x80000) and `streak_line` (0x100000). The list ends with 0x200000.
An unnamed bit is written `bit_N`.

### Definitions

Each definition keeps its XBE `address`. Scalars:
- **Force.** `force_scale`, `force_jitter`, `drag`, `drag_jitter` and
  `mass`. The drag each frame is
  clamp(((rand × drag_jitter + drag) / mass) / 100) × 0.5.
- **Gravity.** `no_gravity` and `gravity`. `gravity_angles` are raw
  radians turning (0, −1, 0).
- **Wind.** `wind_off`, `wind` and `wind_angles` (degrees).
- **Pull.** `pull_off`, `pull`, `pull_angles` (degrees),
  `pull_to_target` and `pull_range`. A negative pull draws a particle in.
- **Life.** `life` and `life_jitter` in seconds.
- **Size.** `size`, `size_jitter` and `scale`.
- **Colour.** `colour`, `colour_jitter_hsv`, `alpha` and `alpha_extra`, as
  bytes.
- **Emission.** `rate` (particles a second) and `rate_jitter`; `spread`
  and `spread_jitter` (degrees of cone); `speed` and `speed_jitter`;
  `push`.
- **Shape.** `shape_type` and `shape_variant` select a placement from table
  0x215b18: point, sphere (filled for type 3) or cluster.
- **Cell.** `cell` is the first atlas cell (+0x114). `frames` (+0x116) is
  how many cells it animates through, at 15 a second.
- **Blend.** `additive` (+0x18) selects additive blending. Glowing cells
  set it; smoke and shards do not.

`curves` holds keyed curves of [time, value] pairs, interpolated linearly
and held at the ends: `red`, `green`, `blue`, `alpha`, `scale`, `pull`,
`wind`, `rate`, `mass`, `speed`, `min_speed` and `max_speed`.
- The `*_time` fields say what a curve's time counts: none, the
  particle's age, the emitter's age, or the percentage of life lived.
- A `rate` curve that ends at zero or less stops its emitter.

## Running

| Limit | Value | Source |
| --- | --- | --- |
| Emitters | 0x90 | |
| Particles per emitter | 0x70 | |
| Spawning stops | above 0x6e live | |

- **Rate.** Each frame an emitter owes rate / frame_rate particles
  (0x0609d8), or the `rate` curve's value over its frames.
- **Stop.** Stopping (0x06516c) takes hold only after the emitter has
  thrown particles.
- **End.** An emitter ends when it is not emitting, has thrown particles
  and has none left alive.
- **Order.** Particles spawn at 0x060b7c and update at 0x061b8f, with
  motion at 0x061d35 and acceleration at 0x063c16. They are drawn by
  0x0625e4.
- **Drawing.** A sprite is size × scale across. A cluster's centre is
  3.5 × larger. A streak runs along the particle's motion in a frame, a
  fifth of that for `streak_short`.

Game code may override an emitter's rate, target or position after it
starts. A supply pickup, for example, sets its burst's rate to 2400 × level
and retargets it every frame.

## Flashes

`flashes` (0x215e48, 4 records of 0x20 bytes, written by `effect_models.py`)
are flip-books of textures from pack 1566. `effect_flashes.cpp` runs them and
`desktop_shots.cpp` draws them. The draw callback 0x065ade puts one texture a
frame on `grid_mdl` (model slot 0x4db, a unit quad). The quad faces the
camera at the group's position and is scaled by the scale the group was
started with (0x057760's, record +0x70).

| Field | Source | Meaning |
| --- | --- | --- |
| `frames` | +0 first texture slot, +4 count | The texture shown at each age. |
| `step` | +8 | Frames the age moves on each frame. The flash ends past its last. |
| `billboard` | +0xc | Faces the camera (all four do). |
| `additive` | +0x10 is 0 | Drawn by 0x0b0124 (flags \| 0x220, SRCALPHA, ONE). Otherwise by 0x0affeb (alpha). |
| `update` | table 0x215ec8 | What changes each frame (below). |
| `draw_flags` | +0x14 | 0x8020, passed to the draw. Kept for reference. |

| Flash | Textures | Update |
| --- | --- | --- |
| 0 fireball | 1566/5–20 | `fade` (0x0666be): opaque, then 17 less alpha a frame over the last 15. |
| 1 smoke | 1566/25–39 | `rise` (0x066770): climbs 0.12 and drifts up to ±0.06 in its own space, which grows by age / (2 × frames) + 1 each frame. Alpha 0x70 − age / frames × 112. |
| 2 glow | 1566/24 | `pulse` (0x0668ab): scale sin(phase) × 0.5 + 1 and a flickering alpha top bit. The phase is shared by every pulse, moves 0.2 a frame and wraps at π. |
| 3 spark | 1566/21–23 | `still` (0x066936). |

Groups 5–12 carry flashes. The pool holds 0x80.

## Shots and impacts

`projectiles` (0x2534a8, 18 looks of 0x2c bytes, by weapon record +0x22)
say how 0x09831e draws a shot. Each look names a mesh of the weapon effects
model (resource 194). The mesh is scaled by `scale` and lies along the
flight, or faces the camera for a billboard. Look 17 is a sprite instead
(1567/1). A look with `trail` also draws the `exhaust` mesh (194 mesh 94)
`exhaust.offset` behind, scaled by `exhaust.scale` and rolled each frame.
Its smoke trail (0x0702d8) is not ported.

`impacts` (0x253408, 13 records of 12 bytes, by weapon record +0x23) name
the effect group a landing starts and the scale of that group's flashes
(0x065f28). A landing starts it where 0x098eda's case for that landing does:
the weapon's `robot_hit` on a robot, its `surface_hit` elsewhere. Cases 1,
2, 4 and 5 start the impact; 0 and 3 do not. `decal` marks the impacts that
leave a bullet hole (`decals`, 0x0663d2). Bullet holes are not ported.

`wreck_groups` are the groups 0x07675e starts where a robot's armour runs
out: 0x34, 0x35 and 0x36 (kept, debris and smoke) and 0xf (the blast). The
supply carrier starts group 0x3f both when shot and when its wreck crashes
(0x06781a and 0x06828e, `crash_effect` in the
supply look).

The port's earlier glow discs for shots and blasts are gone. The Armory's
explosions now only time the blasts' damage.

## Arena fence impacts

Shots keep the closest collision triangle's surface and normal through
their landing event. Surface 8 (including its `% 1000` variants) selects
the arena boundary above the visible fence. When the weapon starts its
surface impact, `0x065f28` also starts `0x07027b`'s fence effect. A closer
robot, prop, or door does not inherit that boundary contact.

`fence` in `tables/effects.json` supplies the dedicated mesh animation:
model resource 197, slots 1021–1050 (meshes 0–29). `0x071869` selects one
mesh per 30 Hz frame, scales X/Z from 0.1 by `age / 30 * 0.05`, scales Y
by 0.2, and places it in the surface frame offset -0.5 along the normal.
`0x07027b` allows twenty simultaneous effects, each lasting thirty frames.
The native animation advances with elapsed game time, independently of
presentation rate and split-screen draws.

The original uses its `effopt` refraction path with the shader sphere at
the contact point and radius zero (`0x071869`, `0x0c05c2`). The port draws
the extracted meshes through its existing scene-copy refraction pass;
the shape, timing, scale and refraction settings remain editable in the
extracted table. No replacement mesh or ripple texture is generated.

## Not yet ported

- Fragments (0x1000) and the detail of swirl (0x8000).
- Debris bouncing. Debris is pinned to the floor instead.
- The exact placement of cluster particles, and the random_offset target.
- The kinds' depth biases in 0x0bf4cf.
- Bullet holes and scorch marks (0x0663d2) and missile smoke trails (0x0702d8).
- Beams (hit case 5) spend themselves on a surface in the port instead of
  flying on.
- The original emitter's first frame carries the velocity of a jump from
  the origin. The port starts emitters still.
