# Custom CHIPs

How the port's added CHIPs (Cat V, Cat VI, Cat VII) are made, and how to
recreate them or add another one exactly the same way. The three are part of
the base game data, not a mod: they appear in BUY CHIP, saves and battles like
the original's 96.

## Where everything lives

| What | File |
| --- | --- |
| Records, stats, shop days | `data/tables/added_chips.json` |
| Face pictures (64x64) | `data/assets/textures/chips/<name>.png` |
| Source photographs | `source-art/chips/<name>.webp` |
| Face recipes | `scripts/chip_faces.json` |
| Face generator | `scripts/make_chip_face.py` (+ `chip_face_look.py`, `chip_face_matte.py`) |
| BUY CHIP help text | `data/ui/chip_shop.json` `chip_help`, at the CHIP's index |
| Code that joins them to the tables | `port/src/added_chips.cpp` |

`data/assets/` is the last asset root (`DataRoots::standard` and `shipped`), so
a face named `textures/chips/cat_v.png` resolves there unless a mod or the
converted assets hold the same path. The release ZIP ships everything under
`data/`, so nothing extra is needed to package them.

## How the records join the game

The converted `tables/parts.json` (from the player's disc) holds the original
96 CHIPs, indices 0-95. `read_converted_table` reads it and appends one record
per family personality from `added_chips.json`, numbered from `first_index`
(96); `tables/garage.json` gets the `shop` offers added to its rotating
`chip_lineups`. Every reader of those two tables goes through it (desktop
screens, training robots, the Rumbling AI world, online checks), so the new
records are ordinary CHIPs everywhere. Nothing is written to the extraction, so
no re-convert or `converted_assets_version` bump is needed.

Rules:

- **Only ever append.** Saves and shop days name CHIPs by number. Add new
  families (or personalities) at the end of `families`; never reorder.
- The additions are skipped when the parts table does not end exactly at
  `first_index` (another game's table) and in S.L.A.I. mode.
- A shop day may hold at most 9 CHIPs; the original days hold 8, so each day
  gets exactly one added offer.

Current numbering: Cat V 96-100, Cat VI 101-105, Cat VII 106-110 (chatter
personalities 0-4 in order).

## 1. The face picture

The original faces are 64x64 cells of
`textures/scroll/fight_chara00_tex/0.png`: an animal on a flat green
"monitor" backdrop, tinted along a green ramp, with dark scanlines and a
two-line frame. `chip_face_look.py` measures all of that from the player's own
extraction each run (nothing is copied from the original pictures):

- **Backdrop**: the median flat green between lines, about (0,142,57).
- **Ramp**: subject brightness to (R,G,B), with the scanlines divided out.
- **Scanlines**: the faces use one of two row sets; the generator uses set B
  (rows 2, 7, 11, 15, 20, 25, 30, 35, 40, 45, 51, 56, 60), the set 13 of the
  17 animals use. Each row's darkening is measured per channel (blue darkens
  less, giving the teal cast); dark rows are about x0.7-0.8, their neighbours
  about x0.92. Using one set only matters: averaging both doubles the lines.
- **Frame**: a bright outer line (about (0,227,8)) and a dark inner line.

Making a face:

1. Copy the photograph to `source-art/chips/<name>.webp`.
2. Add a recipe to `scripts/chip_faces.json`:
   - `crop` `[left, top, right, bottom]` in photo pixels: a square around the
     head and ruff, ears included, roughly as tight as the original animals.
   - `matte`:
     - `"lawn"` for a dark animal on grass, sky or cardboard (Cat V). It grows
       the light/green background in from the edges and keys it out.
     - `"outline"` for anything else (Cat VI, Cat VII): a polygon in `outline`,
       in units of the crop scaled to 512x512, (0,0) top left. Trace about
       10 units outside the fur; the matte refines the edge to the fur within
       12 units either side (colour clusters of the sure-inside and
       sure-outside pixels nearby) and keeps the part joined to the centre.
       Run the polygon along the bottom edge for the chest.
   - `despill_green: true` when the photo was taken on grass, so no green
     reflects off the fur before the tint.
   - `gamma`: the fur's tone curve after levels (1st to 99.7th percentile).
     Under 1 lifts dark fur. Used: black Cat V 0.62, orange Cat VI 0.78, grey
     Cat VII 0.8. Aim for a mean face brightness near the originals' (about
     130-150 on the green channel).
   - `output`: `data/assets/textures/chips/<name>.png`.
3. Run, with previews (cutout on magenta and a 6x face):

```bash
py -3.14 scripts/make_chip_face.py --only "Cat VI" --preview C:/temp/chip_preview
```

Without `--only` it rebuilds every face. The output is deterministic: the
three committed faces regenerate byte-identical. Python 3.14 with Pillow and
numpy is required; do not run it with `-I` (the scripts import each other).

Check the cutout for leftover background (bezels, gutters, a pale fringe) and
move the outline points in or out where it shows; check the 6x face against
the original faces for brightness.

## 2. The stats

Each family's `stats` are the original record fields, exactly as the
converted `parts.json` writes them:

| Field | Meaning |
| --- | --- |
| `inner_shape`, `inner_value` | aiming sight shape (0 square, 1 wide, 2 tall) and size curve |
| `outer_shape`, `outer_value` | lock-on sight shape and size curve |
| `track` | 0..999, how fast the reticle follows a target (**Aim Speed**) |
| `lock_frames` | a lock takes this many 60 Hz frames (**Lock On Time**, in seconds) |
| `critical_percent` | chance of a critical hit, shown as **Mortal Shot** |
| `dodge_percent` | chance of the **Repulsion Shield** (the help texts' name for it) |
| `knockdown_resistance` | resistance to being knocked down (**Knockdown Resist**) |
| `weight` | hundredths of a tonne (22.0 shows as 0.22 t in the shop) |
| `price` | the shop price curve (shown x10 in NY) |

BUY CHIP, SELL CHIP and UPLIFT list the bold names with their values in the
CHIP dialog; the rows, their order and their layout are the dialog's `rows`
in `data/ui/chip_shop.json` and `data/ui/uplift.json`.

A curve is `{base, mid, top, knee, low_scale}` (`LevelCurve::value`): `base`
at level 0, `base + mid` at level `knee`, `base + top` at level 99, linear in
between. `low_scale` applies to negative levels; keep -1 (and 1 for price) as
the original does.

The original cats (Cat I-IV) share the sights (wide lock-on, square aiming
sight, same size curves), knockdown 3 -> 15, weight 20.1 and price; they
differ only in track, lock time, Mortal Shot and shield (table below). The added
cats keep the cat sights, price curve shape and knees, and move the other
stats from the owner's description of the animal:

| Trait | Stat |
| --- | --- |
| Hunts flies well / is fast | higher `track`, fewer `lock_frames` |
| Very fast, agile | higher `dodge_percent` (Repulsion Shield) |
| Aggressive / gentle, soft touch | higher / lower `critical_percent` (Mortal Shot) |
| Big, sturdy / small, indoor | higher / lower `knockdown_resistance`, `weight` |
| Demanding, rare | higher `price` |
| Lazy | lower `track`, more `lock_frames` |
| Talkative | no stat; mention it in the help text |

Resulting values (level 0 -> level 99):

| | Track | Lock | Mortal Shot % | Shield % | Knockdown | Weight | Price (base) |
| --- | --- | --- | --- | --- | --- | --- | --- |
| Cat I, Cat II | 1 | 12 | 2.0 -> 7.5 | 0.5 -> 7.5 | 3.0 -> 15.0 | 20.1 | 6500 |
| Cat III | 1 | 14 | 1.0 -> 6.5 | 0.5 -> 8.0 | 3.0 -> 15.0 | 20.1 | 6500 |
| Cat IV | 2 | 16 | 0.5 -> 6.0 | 0.5 -> 8.5 | 3.0 -> 15.0 | 20.1 | 6500 |
| Cat V Norwegian Forest Cat | 3 | 8 | 0.5 -> 4.0 | 1.0 -> 10.5 | 2.0 -> 11.0 | 22.0 | 6500 |
| Cat VI Maine Coon | 3 | 8 | 2.0 -> 10.0 | 1.0 -> 10.0 | 1.5 -> 9.5 | 20.1 | 7475 (x1.15) |
| Cat VII Ragdoll | 1 | 22 | 0.5 -> 3.5 | 0.5 -> 10.5 (late) | 3.5 -> 18.5 | 24.0 | 8450 (x1.3) |

Keep knees as the original's (50 for sights, critical and price; 60 for
shield and knockdown), and scale a price by multiplying all three of `base`,
`mid` and `top`. Each family's `traits` note in the JSON records why.

## 3. The chatter

A CHIP's `chatter` picks the line table its robot speaks from in battle
(`tables/chatter.json`, converted from the disc). There are 67 tables, each
listing a script for each of 35 events (first kill, shot down, hit from
behind, Repulsion Shield, low armour...); every animal CHIP family uses tables
0-4, one per personality, and the five records of a family differ only in
that. The added cats do the same: `"chatter": [0, 1, 2, 3, 4]` gives five
records per cat with the original cat chatter. Which events speak and how
often comes from `gates` and `chances` in the same table, not from the CHIP.

The face shown in the chatter window and on name entry is the CHIP's `face`,
so the added faces appear there with no extra work.

New chatter lines are not supported yet. They would need a new table appended
to `tables` (index 67 onward) whose 35 entries name new scripts appended to
`scripts`, joined at load the same way the CHIPs are; a family would then list
that table in `chatter`. Lines are full-width text, about 26 characters by 2
lines, with `{code:fff8}` for the robot's name.

## 4. Shop

`shop` lists `{day, chip, chatter, level}` offers: day 0-23 of the rotating
`chip_lineups`, the family `name` and personality, and the level it is sold
at (0, as all original offers). The current rotation is day `d` -> family
`d % 3` (V, VI, VII) with personality `(d / 3) % 5`, so each cat and
personality comes round in turn and every day sells one added cat beside the
original two.

`data/ui/chip_shop.json` `chip_help` holds the help line for each index; add
one string per record (the five of a family alike, as the originals are), at
most about 235 characters, in the original wording, ending
"Modeled after <breed>.".

## Adding another CHIP, step by step

1. Photograph -> `source-art/chips/`, recipe -> `scripts/chip_faces.json`, run
   `make_chip_face.py` (section 1).
2. Append a family to `families` in `data/tables/added_chips.json` with
   `name`, `breed`, `face`, `chatter`, `traits` and `stats` (section 2). Its
   records number on from the last (the next is 111).
3. Append its `shop` offers (at most one more per day, 9 in all).
4. Append one `chip_help` string per record to `data/ui/chip_shop.json`.
5. Build and run `phantomcrash_core_tests` (`added_chips_tests.cpp` checks the
   numbering, the rotation and some values; update it for the new family).
