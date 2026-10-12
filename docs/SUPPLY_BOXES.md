# Supply boxes

The supply carrier drops pickups ("boxes") whose kinds are data. The
original game has ten kinds; the port keeps them as they are and appends
three top money tiers, and gives any kind its own picture, colours, glow
and pickup sound. These additions are part of the base game data, not a mod.

## Where everything lives

| What | File |
| --- | --- |
| Kinds, chances, looks | `data/world/training.json` `battle.supply.kinds` |
| Crate pictures | `data/assets/textures/supply/money_<NY>.png` |
| Crate generator | `scripts/make_supply_crates.py` |
| Parse | `port/src/supply_settings.cpp` |
| Roll and take | `port/src/supply_drop.cpp`, `port/src/battle_supply.cpp` |
| Drawing | `port/src/desktop_supply.cpp`, `port/src/desktop_supply_kinds.cpp` |
| Two-colour tint, sheen | `port/src/desktop_world_shaders.cpp` (`label_colour`, `reflect_modes.y`) |

## The shipped boxes

Money `amount` is in stored units, tens of NY (`money.hpp`): 10000 shows as
100,000 NY. A rental robot's prize is halved at RESULT as usual.

| Kind | Box | Chance | Body | Lettering | Glow | Extra |
| --- | --- | --- | --- | --- | --- | --- |
| 0 | 1,000 NY | 6.25% | original blue | original | blue | |
| 1 | 5,000 NY | 12% | original blue | original | blue | |
| 2 | 10,000 NY | 8% | original blue | original | blue | |
| 3 | 100,000 NY | 2% | `#8A4FFF` violet | white | violet | |
| 4-6 | repair 10/30/50% | 15/10/10% | original | original | grey | |
| 7-9 | ammo 30/50/100% | 6/15/14% | original | original | green | |
| 10 | 250,000 NY | 1% | `#FF8A1E` orange | white | orange | |
| 11 | 500,000 NY | 0.5% | `#FFC83A` gold | `#4A2A08` brown | gold | sheen 0.6 |
| 12 | 1,000,000 NY | 0.25% | `#FFD54A` gold | `#B01818` red | every hue | sheen 1, glow x1.5, `spin_pay` |

The original's chances sum to 1; the 1,000 NY box gave up the 1.75% the
three new tiers take (8% to 6.25%). With three boxes a trip, a jackpot
comes on about 1 trip in 133.

## A kind's fields

```json
{ "effect": "money", "amount": 100000, "chance": 0.0025, "crate": 3,
  "image": "textures/supply/money_1000000.png",
  "tint": "#FFD54A", "label_tint": "#B01818", "sheen": 1.0,
  "glow_scale": 1.5, "glow_cycle": 0.75,
  "take_sounds": ["supply_take", "spin_pay"] }
```

| Field | Meaning | Default |
| --- | --- | --- |
| `effect` | `money`, `repair` or `ammo` | `money` |
| `amount` | stored money units, or the share of armour or magazine | 0 |
| `chance` | odds of each pickup carried; the rolls run in list order | the original's for kinds 0-9, else 0 |
| `image` | a crate picture replacing the crate's own | none |
| `crate` | which original crate (0-9) its mesh is | its own number; 0 for an added kind |
| `tint` | body colour, `#rrggbb` or `[r, g, b]`, multiplying the picture | none (the picture as it is) |
| `label_tint` | lettering colour, where the picture's alpha is below 1 | none |
| `glow` | the glow's colour | the tint, else the effect's |
| `glow_scale` | the glow's size, times the look's `glow_size` | 1 |
| `glow_cycle` | hue turns a second (0: a fixed colour) | 0 |
| `sheen` | strength of a metal rim and sun glint in the tint | 0 |
| `take_sounds` | named sounds (`data/audio/sfx.json`) played together on a take | the look's `take_sound` |

A pickup's kind is its place in the list (the battle event's `slot`, the AI,
replays and online state all use it), so **only ever append**; to retire a
kind set its chance to 0. Chances should sum to at most 1; when they sum to
less, the remainder falls to kind 0, as in the original's roll.

The computer robots leave money to the player at ability 31 and above
(`robot_ai.json` `pickup_money_ability`); the port tests the kind's effect,
where the original tested the kind's number, so added money kinds count.

On a split screen the original tints a box by its effect's colour, blinking,
in place of the glow; a kind with a tint blinks in its tint instead.

The S.L.A.I. stages (`supply_pickups` in `stages.json`) draw kinds 0-9 with
their own crate and pictures, untinted; added kinds are drawn on the
original's crate there.

## Crate pictures

`make_supply_crates.py` draws each crate from scratch in vector shapes at
128x128 on the original crate's layout (rails, clamps, panel), so the crate
mesh's UVs fit. Nothing is copied from the extracted art. The body is
greyscale for `tint`; the lettering is white with alpha 0 for `label_tint`.

```bash
python scripts/make_supply_crates.py --amounts 100000 250000 500000 1000000
```

It needs Pillow and the Windows fonts Times New Roman Bold ("NY") and Arial
Narrow Bold (the amount). A picture of your own works the same way: any
size, greyscale body, lettering marked by alpha below 1; leave alpha at 1
throughout and `label_tint` does nothing.

## Adding a box

1. Append a kind to `battle.supply.kinds` with its `effect`, `amount` and
   `chance`, taking that chance from another kind.
2. For a new money tier, run the generator with its NY amount and point
   `image` at the result; pick `crate` 3 (the 100,000 crate) for a money box.
3. Choose `tint`, `label_tint` and, for something special, `sheen`,
   `glow_scale`, `glow_cycle` and `take_sounds`.

A new effect (a shield, a speed boost) needs code: a new `SupplyEffect`,
its case in `Battle::take_supply`, its parse name in `supply_settings.cpp`,
and a `glow_colours`/`take_effects` entry or a kind `glow`.
