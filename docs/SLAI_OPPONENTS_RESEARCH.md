# S.L.A.I. campaign opponents

Which robots S.L.A.I.'s campaign rumbles field, how they are chosen, and how
their parts map onto the port's imported S.L.A.I. parts. The plan is in
`docs/SLAI_RUMBLE_PLAN.md`; the match and regulation context is in
`docs/SLAI_CAMPAIGN_RESEARCH.md` (branch `claude/hungry-cannon-662df3`).

Code:

- `src/phantomcrash_research/slai/opponents.py`: table decoders.
- `src/phantomcrash_research/slai/opponent_rules.py`: candidate selection
  and line-up picking, mirrored from the executable.
- `src/phantomcrash_research/slai/opponent_export.py`: names, builds and port
  parts resolved into JSON-ready data (`export_opponents`).
- `tests/test_slai_opponents.py`: synthetic layout and rule tests, plus tests
  that need the local disc.

## Provenance

All addresses are virtual addresses in the PAL main executable SLES_529.40.
`opponents.guard` checks the code words that fix each table's geometry and
refuses to run on any other release.

Names come from four different sources, and they are kept apart here:

- **Shipped text.** Pilot names come from `CHARANAME.MSG`. Part, CPU and
  option names come from `UK_BODYNAME`, `UK_LEGNAME`, `UK_ARMNAME`,
  `UK_SHOULDERNAME`, `UK_CPUNAME` and `UK_OPTIONNAME.MSG`. Examples are
  "Onoda-3", "PROTON-K7P2 Body" and "Dog-3".
- **Function labels.** Names like `f_22A070` are only addresses.
- **Researcher-assigned labels.** Every table name and field name in this
  document and in the code is mine, not an original symbol: "pilot", "robot",
  "roster", "leader", "cameo", "pool", "behaviour" and so on.
- **Confidence.** Fields are marked **High** (the access is traced in code and
  the values are consistent), **Medium** (traced, but the meaning is
  inferred) or **Low** (the meaning is a guess).

## Opponents

### The chain

```
match row (0x41ECE0) ─ record ──▶ roster row (0x420F40) ─ u16 id ─▶ [substitution 0x422C60]
                    ─ record_b ─▶ roster row (cameo list)                    │
                    ─ pool ─────▶ roster row (filler pool)                   ▼
                                                     opponent row (0x37E600)
                                                     ├─ pilot ─▶ pilot row (0x374280) ─▶ name refs (0x371A20) ─▶ CHARANAME.MSG
                                                     └─ robot ─▶ robot row (0x382390) ─▶ part tables ─▶ port parts
```

`f_229CE0` builds a rumble entrant from one opponent row:

- `f_20B970` copies the robot.
- `f_223B40` copies the pilot to entrant +0x9C.
- The level goes to entrant +0xBC and the behaviour to +0xBE.
- `f_229DA0` evaluates the pilot's +8 curve at the level and stores the
  result at entrant +0xC4.
- `f_225C1C` later passes (level, behaviour) to the spawn setup `f_242820`.

### Tables

| Table | Address | Rows | Stride | Accessor |
|---|---|---|---|---|
| Name references | 0x371A20 | 2583 | 4 | `f_22A070` |
| Pilots | 0x374280 | 1272 | 32 | `f_22A0A0` |
| Opponents | 0x37E600 | 1969 | 8 | `f_248780` |
| Robots | 0x382390 | 1887 | 156 | `f_2487A0`, copied by `f_20B970` |
| Bodies | 0x3CA320 | 35 | 60 | `f_203470` range check |
| Legs | 0x3CAB60 | 90 | 208 | same |
| Arm weapons | 0x3CF480 | 102 | 312 | same; `f_24D760` slots 0–1 |
| Shoulder weapons | 0x3D70D0 | 107 | 312 | same; `f_24D760` slots 2–3 |
| CPU chips | 0x3DF340 | 452 | 556 | `f_24D440` |
| Options | 0x41C8F0 | 108 | 56 | `f_24D5E0` |
| Leg-type jump table | 0x41EA00 | 35 | 4 | `f_222A60` |
| Matches | 0x41ECE0 | 157 | 56 | campaign research |
| Rosters | 0x420F40 | 210 | 30 | `f_2225A0`, `f_2240E0` |
| Regulations | 0x4227E0 | 32 | 36 | campaign research |
| Substitutions | 0x422C60 | 6 | 16 | `f_256CC0` |
| Status jump table | 0x371450 | 31 | 4 | `f_2225A0` |
| Leader-completion jump table | 0x3714F0 | 31 | 4 | `f_222F70` |
| Special roster (record 0x7D) | 0x30A090 | – | 30 | `f_2225A0` |

The part selectors' range check (`f_203470`) is inclusive, so every part
table has one row more than its upper bound: body 0..34, legs 0..89, arm
0..101 and shoulder 0..106.

### Opponent row (8 bytes, 0x37E600)

| Offset | Type | Field | Confidence |
|---|---|---|---|
| +0 | s16 | pilot row | High |
| +2 | s16 | robot row | High |
| +4 | s16 | behaviour, 0..14; written to entrant +0xBE and passed to `f_242820` | Medium (AI or spawn style, unresolved) |
| +6 | s16 | level, 1..65; written to entrant +0xBC and the pilot-curve input | High |

### Pilot row (32 bytes, 0x374280)

| Offset | Type | Field | Confidence |
|---|---|---|---|
| +0 | s16 | name reference (through 0x371A20) | High |
| +2 | s16 | second name reference (handle or alias) | High |
| +4 | s16 | −1 or 0..88 | Unknown |
| +8 | u32 ×3, s8 ×2 | level curve {base, mid, top, knee, low_scale}, evaluated by `f_24CBC0` at level/99 | High layout, Medium meaning |
| +0x18 | s16 | 0..115, collected into rumble +0xF18 by `f_223100` | Low |
| +0x1A, +0x1C | s16 | | Unknown |

The pilot curve's base runs from 10 to 22500. The result lands at entrant
+0xC4, which may be the prize money or the rating; this is unresolved.

A name-reference row is {s16 CHARANAME index (−1 for none), u16 second
value}. The second value is unknown, possibly a portrait or a voice. The
pilot's references are not direct CHARANAME indices. For example, reference
1 is "ENEMY", 500 is "Reese"/"Easter" and 1270 is "DD"/"JJ".

`CHARANAME.MSG` has 2,288 strings, which is more than `music.messages`
allows, so `opponents.read_names` reads it instead. Glyph units below 0x5F
are ASCII minus 0x20.

### Robot row (156 bytes, 0x382390)

This is the same build struct the player's garage uses: `f_20B970` copies it
whole.

| Offset | Type | Field | Confidence |
|---|---|---|---|
| +0 | s16, s16 | body: part id and part level (−99..99) | High |
| +4 | s16, s16 | legs | High |
| +8, +0xC | s16, s16 | arm weapons, slots 0 and 1 | High |
| +0x10, +0x14 | s16, s16 | shoulder weapons, slots 2 and 3 | High |
| +0x18 | s16 | CPU chip row | High |
| +0x1A | s16 ×16 | paint ids | Low |
| +0x3A | s16 ×3 | always 9999, 9999, 0 | Unknown |
| +0x40 | f32 ×16 | paint values | Low |
| +0x80 | s16 | emblem (`f_259770`, a 0x1480-byte image copy; ids ≥ 57 are special) | Low |
| +0x82 | s16 ×2 | | Unknown |
| +0x86 | s16 ×10 | options (−1 for empty) | High |
| +0x9A | s16 | 140 or 151..160 | Unknown |

An empty part slot is id −1. A single arm weapon is always in slot 0.

CPUs 0–49 are the named shop chips. CPUs from 386 upwards are only used by
non-player robots ("Unknown" or animal names such as "Dog-3"). A CPU row is
556 bytes of AI parameters, and its first s16 is the UK_CPUNAME index. Only
+0x228 is traced so far (`f_229C20`); the rest of the layout is unknown.

Non-player robots carry options 105/106, which are both named "Dummy" in
UK_OPTIONNAME.

### Roster row (30 bytes, 0x420F40)

The row holds 15 u16 values:

- **[0]** is the leader.
- **[1..14]** are the members.
- **0xFFFF** marks an empty slot.

Save record byte 0 says the leader is beaten; bytes 1..14 are the members'
beaten flags. Free-class rosters have no leader, and regional rosters have
only a leader.

### Match fields used here (0x41ECE0, 56 bytes)

| Offset | Field | Notes |
|---|---|---|
| +0 | regulation | |
| +4 | night | |
| +6, +7 | robots_min, robots_max | max is capped at 6 |
| +8 | pool roster | Filler drones: Free 2, D 1, C 2, B 3, A 4, master 5, restriction 2, 139 special, −1 for head-to-head. This is not a level. |
| +0xA | record | roster for the members and the leader; also the save record |
| +0xC | record_b | second roster, the "cameo" list |

Record 0x7D reads the special roster at 0x30A090 instead. Its first member
is 125.

Matches 127 and 144 have no record, record_b or pool, so this path gives
them no candidates. They are probably scripted fights. The scripted battles
in battledvd at 0x5276E0 use match ids 0x80..0x84 and fixed opponent ids:
0x39 and 0x52F..0x533.

### Substitution (`f_256CC0`, 0x422C60)

The table has six rows of {replacement, 7 ids}. The first row that lists an
id decides the result:

- If its story-flag condition holds (`f_204E50`), the id is replaced by
  1426..1431.
- Otherwise the id stays unchanged.

The conditions per row:

- Row 0 applies while flag 0x16 is clear.
- Row 1 applies while 0x16 is set and 0x24 is clear.
- Row 2 applies while 0x24 is set and 0x25 is clear.
- Rows 3–5 apply while 0x25 is clear.

What these story flags mean is unknown.

### Candidate lists (`f_2225A0`, constructor arguments 0, 0)

`Rules.candidates` builds five lists, at most 30 entries of 12 bytes each.
Each roster id is substituted and then checked for eligibility with
`f_222A60` against the match's regulation:

- **unbeaten / beaten.** Members of `record`, split by their beaten flag.
- **cameo.** Members of `record_b`. Regulation 5 sorts them into
  unbeaten/beaten instead.
- **leader.** The leader of `record`, once the leader is ready. If record
  byte 0 is 1 (already beaten), the leader goes to the beaten list instead.
- **pool.** Members of the `pool` roster.

The leader is ready (`f_222F70`) when either holds:

- the record status is 3; or
- the status is 2 (every flag set) and the regulation's completion rule
  allows it.

The status rule (jump table 0x371450) depends on the regulation:

| Regulations | Status rule |
|---|---|
| 0 | first 5 member flags |
| 6–21, 23–26 | all 14 member flags |
| 1–5, 22 (Free), 27–29 (regional and finals) | always open (status 3) |
| 30 | status 2 |

The flag-based completion rule also needs a runtime session check:
`f_224830() × f_2230C0() ≥ 2`. Regulation 30 needs a counter ≥ 5 instead.

Eligibility (`f_222A60`):

1. Body maker 8 or 9 is always eligible.
2. The body maker must match the regulation's maker (10 means any).
3. The leg type, read from the legs record's +6 low byte through jump table
   0x41EA00, must fit one of the four leg classes: 11 means any; 12 means
   types 0–2, 13 types 3–6, 14 types 7–10.
4. Special rules:

   | Special | Requirement |
   |---|---|
   | 1 | option +0x1A sum ≤ 0 |
   | 2 | no arm weapons |
   | 3 | no shoulder weapons |
   | 4 | every weapon's +0x134 is 2 |
   | 5 | every weapon's +0x18 is 1 or 9 |
   | 6 | every weapon's +0x134 is 3 or 15 |
   | 7 | CPU 0 or 1 |

### Line-up (`f_2232E0` / `f_223420`, `pick_lineup`)

Slot 0 is the player. For slots 1..5:

- No further robot is taken once the total reaches robots_max.
- Slots 1–3 draw from the main lists while main draws are below robots_min
  and below 4. The order of preference:
  1. the leader, when one is ready;
  2. a cameo, with chance 1/64;
  3. unbeaten: always if fewer than 4 beaten remain, otherwise 95% unbeaten
     and 5% beaten;
  4. beaten.
- Slots 4–5 draw from the pool, at most twice.

Each draw takes a uniform index among the first `min(n, 3)` remaining
entries of the chosen list, using `rand()/32768` (`f_249C50`). Roster order
therefore matters: early members appear first.

### Port part mapping (`port_part_map`)

The port's importer registers one port part per shipped model
(`mods/slai/native_parts.json`, `source_record`). The other S.L.A.I. records
that share the model are variants with the same name and maker, so they
resolve to the same port part. The model keys are:

- **Body:** its name index. Bodies 10–34 repeat bodies 0–9.
- **Legs:** the u16 at +4.
- **Weapons:** the u32 at +4.

All 1,805 opponents referenced by rosters and substitutions resolve every
part slot to a port part.

The shipped name for drone weapons is blank. For example, arm 93 → port 123
and shoulders 94/95 → port 115/116.

The port part numbers here and in the examples below predate two changes
to the registration: the parts fitted to those Phantom Crash sells, and the
corrected model-family makers (`native_parts.py` `SOURCE_MAKERS`,
2026-10-26). Registration now writes the current mapping into
`tables/slai_rumble.json` "opponents" (`slai/opponent_table.py`); read the
numbers there, not here.

### In the port

The port fields these line-ups in S.L.A.I.'s rumbles. See
SLAI_RUMBLE_PLAN.md, "Armed opponents from the original's rosters".
`slai_lineup.cpp` reimplements the check, the lists and the draw, and is
tested against this mirror through `slai/lineup_fixture.py`.

### Example: Tokyo, CLASS FREE, day (match 78)

The match uses regulation 22 with 3..6 robots, pool roster 2, record 114 and
record_b 143. Its status is 3, but roster 114 has no leader.

On a fresh save the lists hold 14 unbeaten, 9 cameo, 0 leader, 0 beaten and
10 pool entries. Each part cell reads `name id Llevel → port part`. Pilot
names are shipped CHARANAME text.

| List | Opp. | Pilot | Lv | Beh. | Body | Legs | Arms | Shoulders | CPU |
|---|---|---|---|---|---|---|---|---|---|
| unbeaten | 525 | Onoda-3 / KenKen6 | 3 | 6 | PROTON-K7P2 Body 10 L−99→16 | PROTON-3Wheels 38 L99→43 | T48 Machine Pistol 47 L−50→77 ×2 | T50 Grenade 48 L0→69 | 406 Dog-3 |
| unbeaten | 526 | Natsuko / Ambrose | 3 | 2 | ZWERG-EV.3 Body 13 L−38→20 | ZWERG-4Baller 58 L47→69 | SVabwehr Kanone5 77→115, Maschinenpistole17 71→109 | Zwerg K-Panzerung 79→108 | 448 Rabbit-2 |
| unbeaten | 527 | Itou / Brian | 3 | 4 | PROTON-K7P2 Body 10 L−10→16 | PROTON-Hover Jet 40 L89→60 | T67 Grenade M.Gun 51→81 ×2 | –, T50 Grenade 48 L56→69 | 420 Bird-7 |
| unbeaten | 528 | Akinobu / Brin | 14 | 0 | KHT-O.279 Body 11 L−6→19 | KHT-Road Roller 45→47 | APG65 "Perun" 58→104, PPk51 "Kirsanoff" 55→101 | PRK57-1 "Ovinnik" 57→94 ×2 | 435 Dolphin |
| unbeaten | 529 | Satoru / MoeMoeF3 | 3 | 4 | HARTMAN-IP1 Body 14 L−35→17 | HARTMAN-2Wheels 61→54 | M-32 Assault Rifle 80→86, M-2019 Commercial 79→85 | M-2 SRAM Launcher 81→78, M-18 Heavy M.Gun 82→79 | 430 Ape-2 |
| unbeaten | 530–534 | Kawada, Otomo, Maki, Nakamura, Tetsuya | 14/14/14/14/25 | | ZWERG, KHT, CARRO, ZWERG, KHT | | | | |
| unbeaten | 1498–1501 | Nozomi, Ogawa, Shibano, Mariko | 22/22/22/29 | | HARTMAN, CARRO, KHT, CARRO | | | | |
| cameo | 1073, 1088, 564, 579, 1043, 1058, 594, 549, 1302 | Ikematsu, Molly, Jasmine, Shizue, Hiroshi, Takayuki, Number41, Kuro, Kirin | 36, 43, 29, 36, 22, 29, 43, 22, 50 | | | | | | |
| pool | 34–36, 47, 48 | DUSTER | 5 | 4 | DUSTER Body 18 L10→24 | DUSTER-3Wheel 68 L±5→76 | arm 93 L±5→123 | – | 0 E-Chip |
| pool | 60, 61, 72–74 | SWEEPER | 3 | 4 | SWEEPER Body 19 L0→25 | SWEEPER-3Wheel 69 L0→77 | – | shoulders 95→116, 94→115 | 0 E-Chip |

All Free-roster robots carry options 105/105, with 106 added from 534
onwards. A typical draw is three of the first members (525–527 early on),
plus two pool drones.

### Example: Tokyo regional head-to-head (match 83)

The match uses regulation 27 with 1..2 robots, no pool, record 107 and
status 3 (always open). Roster 107 has only a leader, opponent 595:

| Field | Value |
|---|---|
| Pilot | Masami / Virginia |
| Level / behaviour | 51 / 4 |
| Body | PROTON-K7P2 Body 30 L50 → port 16 |
| Legs | PROTON-Hover Jet 82 L99 → port 60 |
| Arms | T64 Sawn-off Gun 3 L−35 → 80; T67 Grenade M.Gun 4 L0 → 81 |
| Shoulders | T70 DL-ASV Missile 3 L−25 → 72; T66 Anti-SV Rocket 4 L65 → 73 |
| CPU | 390 ("Unknown" in UK_CPUNAME) |
| Options | 105, 106, 106 |

The line-up is the player plus Masami.

### Still unknown

- **Opponent behaviour (+4).** What the 0..14 values select in `f_242820`.
- **Pilot fields.** +4, +0x18, +0x1A and +0x1C are unknown. The meaning of
  the level-curve result at entrant +0xC4 is also open: prize money or
  rating.
- **Name-reference second value.** Possibly a portrait or a voice.
- **Robot fields.** Paint (+0x1A, +0x40), emblem (+0x80), +0x3A, +0x82 and
  +0x9A.
- **CPU rows.** The 556-byte AI parameter layout is unknown.
- **Leader session check.** The flag-based leader rule depends on
  `f_224830() × f_2230C0() ≥ 2`; `Rules.leader_ready` takes it as a
  `session_ok` input.
- **Other constructors.** The second candidate constructor (0x222270) and the
  non-zero `f_2225A0` constructor arguments are untraced.
- **Special slots and modes.** Match 0x7E's special slot rules, the
  save+0x4768 == 3 mode and the `f_20CB00` debug path are untraced.
- **Story flags.** What flags 0x16, 0x24 and 0x25 mean, which gate the
  substitutions.
- **Runtime confirmation.** Nothing has been checked in PCSX2 yet. The lists
  and draws are static mirrors.

### Follow-up outside this task

`stats.py` (`TABLES`) reads 34/89/101/106 rows. Because the selectors' range
check is inclusive, it drops the last record of each part table: body 34,
legs 89, arm 101 and shoulder 106.
