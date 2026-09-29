# Battle sounds

Almost every battle sound in the original is a DirectSound 3D voice, placed
where it happens and heard from the camera. The port does the same. The
table is `data/audio/battle_sounds.json`; the code is
`port/include/phantomcrash/battle_sounds.hpp` (what a battle sounds like),
`world_sound.hpp` (placing a sound) and `effect_mixer.hpp` (the voices).

## Banks

The extractor writes each bank's entries as
`sound/sfx/<bank>/NN.wav`, with NN the entry number in decimal.

| Bank | File | Used by |
|---|---|---|
| 0 | `sys` | the menus, READY/GO |
| 1 | `battle` | the computer's robots, some shared sounds |
| 2 | `battle_old` | the computer's plain shots and missiles |
| 3 | `battle_player` | the players' robots, doors, pickups |

A robot whose +8 is -1 is the computer's. The original plays a player's
sound through 0x09df75 and a computer robot's through 0x0493c0; both place
it at robot +0xb0. In the split screen, 0x09df75 plays sounds unplaced, so
the port's split screen hears every sound whole.

## Distance

0x0ddc50 and 0x0de390 place a voice and the listener at half the world's
distances (0x17accc). Every voice has a minimum distance of 1 (0x0ddd4f),
and the listener has a rolloff factor of 0.1 and no Doppler (0x0de616).
Under DirectSound's inverse distance law, a sound is at full volume within
2 world units and at half volume 22 units away, and there is no maximum
distance. The port pans a sound by the side of the camera it is on.

## Robots

0x093637 runs every tick for each robot:

| Sound | Entry | Where |
|---|---|---|
| step | 2 | 0x093cf0 (tables 0x249818, 0x249830) |
| landing | 3 | 0x093df2 |
| jump | 6 | 0x08ea5b, 0x08d7fa |
| dash | 4, then 5 looping with the robot until the dash ends | 0x0945c1 |
| dodge | 7 (motions 13, 14) | 0x094a8d |
| air dodge | 0x13, players only (motions 15, 16) | 0x094a8d |
| shot down | bank 1's 0x10, for every robot | 0x07675e |
| low armour | 0x27 every 0x20 ticks below a quarter of the armour (0x199fd4), players only | 0x093637 |

The original times steps to its walk motions' frames. The port counts
them by time instead (`footstep_seconds`), faster as the walk speeds up.

## Weapons

A weapon's muzzle sound (0x098bb7 → 0x099c02) comes from the handler its
record's +0x21 selects in table 0x2537c0. The extractor writes that byte as
`fire_sound` in `tables/parts.json`.

| +0x21 | Handler | Sound |
|---|---|---|
| 0 | 0x099d26 | by mount and model (+8): shoulder models 1/11/21/31 → 0x19, 6/16/26/34 → 0x1b; arm models 8/29 → 0x1c, 32 → 0x1d; otherwise the plain shot 0x2c (bank 2's 0 for the computer) |
| 1 | 0x09a0da | the plain shot |
| 2 | 0x09a15e | a grenade (HUD kind 3) → 0x18, a missile → 0x30 (bank 2's 2) |
| 3 | 0x09a3ab | 0x18 |
| 4 | 0x09a409 | 0x17 |
| 5 | 0x09b9fe (the beam's update) | 0x2f (bank 2's 1) |
| 6 | 0x09c337 (the melee update) | the swing, by model |

A landing shot or strike sounds too. A melee strike plays 0x1e (bank 1's
0x26 for the computer) (0x098eda, impact kind 4). Other shots play bank 1's
entries whoever fired them (0x047ed9): a missile 0x0f, the straight shots of
models 6/16/26/34 0x0e, and the rest 0x2e. Beams and lobbed grenades make
no landing sound.

The rules in `fire` and `impact` are tried in order, and the first match
wins. A rule can match on `style` (+0x21), `mount`, `models`, `hud_kind` and
`flight`. A weapon without a record plays `fire_fallback`.

## Other sounds

The gate doors (3/0x31 as a door moves, 3/0x32 as it opens) play at their
door. A supply pickup (3/0x26, 0x066e50) plays at the robot that takes it.
READY/GO and the chatter's opening sound play unplaced, as in the original.

## Not yet placed

- 0x0949a0 (3/0xe on, 3/0xf off) and the +0x3494 states of 0x093c5d
  (3/0xc, 3/0x10, 3/0xd).
- The weapon-mount warning 3/0x25 (every 5 s while +0x8c or +0x90 is 4).
- The loop 3/0x2a while a flag in +0x8a2 is set.
- The stage objects' sounds (0x07271e-0x07415d).
- 0x094d86's landing sound by the floor's surface: 3/0x11 on surfaces 5
  and 7 (flags 0x1400), 3/0x12 on surface 6 (0x800), and the robot's own
  landing sound otherwise. The training ground's floor is all surface 9,
  so there the port's `landing` is what the original plays.
