# S.L.A.I. Rumble: a second calendar in Club Wired (plan, 2026-10-20)

Goal: play S.L.A.I.'s own campaign battles inside the port's Club Wired,
on S.L.A.I.'s original 100-day calendar, beside Phantom Crash's 60-day one.
Battles only first; S.L.A.I.'s story stays optional later content.

Sources: `docs/SLAI_CAMPAIGN_RESEARCH.md`, `docs/SLAI_OPPONENTS_RESEARCH.md`,
`slai/campaign.py` and `slai/rumble_table.py`. Port references:
`contests.hpp`, `rumbling_rules.hpp`, `schedule.hpp`, `settings.hpp`
(`QuestSave`), `data/ui/club_wired.json`, `data/ui/schedule.json`.

## How the two campaigns compare

| | Phantom Crash (port today) | S.L.A.I. (recovered) |
|---|---|---|
| Cycle | 60 days (`tables/town.json` calendar) | 100 days (day table 0x42D0F0) |
| Places | 3 venues a day | 7 cities, each one rumble a day, day or night slot |
| Rules | contest class: fee, maker, legs, weight, O.C ban | regulation (32 rows): entry value, maker, 4 leg classes, special rule, prize |
| What is fought | contest roster drawn from teams (live area) | match template (157 rows): regulation, robots 3..6 or 1..2, level, records |
| Progress | class leaders → Area Ranker → First Ranker | stage ladder 0..4: tutorial → 56 ranker records → 7 regional → 3 finals → Master Rank |
| Per-contest record | won flag + team places beaten | 15-byte record: cleared + 14 opponent beaten flags |
| Finals | First Ranker | 3rd/2nd place and final injected in Tokyo, London, New York |
| Shops | dealers/Wild Machines by day | shop, chip and module lineups by day and city |

The shapes match closely: a day-indexed table of what is on offer, rule rows,
and per-contest beaten checklists. The port's calendar and schedule code is
the template; S.L.A.I. needs its own tables and a 7-row schedule, not a new
engine.

## Structure

### Data (all editable; the mod overlay applies path-for-path)

Extracted from the disc by the installer, so nothing is copied into git:

- `artifacts/slai/assets/tables/slai_rumble.json`
  - `regulations[32]`: id, name (UK_RUMBLETYPENAME.MSG), entry value, maker,
    leg classes, special rule, param, prize.
  - `matches[157]`: regulation, night, robots min/max, level, record ids,
    the 10 weights (kept raw until understood).
  - `rota.days[100]`: shop lineups and the seven city pattern ids.
  - `rota.city_days[700]`: five stages × (day, night) match ids, shop lineups.
  - `finals`: host cities, injected matches, fallback table 0x334A80.
  - `rosters[]` / `opponents[]`: once decoded (see the critical path).
  - `provenance`: guarded addresses, as the other S.L.A.I. tables do.

Hand-written port data:

- `data/world/slai_rumble.json`
  - city → stage ids (day/night), using the imported `slai_day_*`/`slai_night_*`;
  - S.L.A.I. maker ids (Carro, Proton, Hartman, KHT, Zwerg) → part makers;
  - leg classes 11–14 → the port's leg types;
  - the stage-ladder thresholds (recovered values, stated as data);
  - balance knobs, defaulting to the original's values: entry value, prize
    scale, opponent levels, robot count.
- `data/ui/slai_rumble.json`, `data/ui/slai_schedule.json`: the screens.

### Save

A new `slai_rumble` block in `QuestSave` (and `quest_json.cpp`), present only
in the S.L.A.I. content profile, which already keeps its own saves:

- `day` (0..99), `stage` (0..4), `city`, `finals_cleared`;
- `records`: record id → cleared + 14 beaten flags (the original's 15 bytes);
- sponsor tracks once they are used.

Older saves load with the block empty, at day 0 and stage 0.

### Logic (pure, testable; no UI)

`slai_rumble.hpp/.cpp` and `slai_rumble_rules.cpp`:

- `scheduled_match(day, city, stage, slot, finals_cleared)`, mirroring 0x257070
  (already mirrored in `campaign.py`, which gives the native tests their
  expected values);
- `next_stage(...)` (0x25A4E0);
- the entry check (0x259A40) against the ridden robot;
- the opponent check (0x222A60);
- record updates and status (0x223ED0), and prize payout.

### Battle

A S.L.A.I. match is an arena battle, not a live Rumbling area. It reuses the
port's arena battle path (as VS and Test Drive do) on the city's imported
stage, with the match's robot count and day or night lighting. Results
update the record and pay the prize, then return to the S.L.A.I. Rumble hub.

### Screens

- **Club Wired:** a new wedge, "S.L.A.I. RUMBLE", shown only with S.L.A.I.
  content. The wheel is authored for six items modulo six, so it must take the
  item count from data. The wedge label is sharp text on an existing wedge
  image; the original pictures stay untouched.
- **S.L.A.I. Rumble hub:** today's day and night rumbles in the current city,
  a city picker and entry. It reuses the contest-info popup for rules and
  prize.
- **S.L.A.I. schedule:** 4 days × 7 cities in the original's display order
  (2 6 0 4 5 1 3), with cleared marks. It is built from the port's schedule
  screen with seven rows.

### Days

Sleeping advances the Phantom Crash day and the S.L.A.I. day together. The
two cycles stay independent (60 and 100 days). Skip-to-day applies to the
schedule that was open.

## Phases (one session each; files do not overlap within a phase)

1. **Merge and export.** Merge `claude/hungry-cannon-662df3`, add the
   `slai_rumble.json` export to registration, and test the tables against
   `campaign.py`.
   - **2 (in parallel): opponent research.** Decode the roster table
     0x420F40 and the builds behind f_256CC0, mapping their part ids to the
     imported S.L.A.I. parts. This is the critical path. PCSX2 now boots the
     game for runtime checks.
2. **Rules core.** Save block, rota lookup, finals, stage ladder, entry check,
   records, prizes, and native tests against `campaign.py` values.
3. **Battle.** Start a match on the city stage with real opponents (or a
   labelled adapter until phase 1b lands), and write results back to the
   save.
4. **Screens.** Club Wired wedge, hub, schedule and sleep integration, with
   visual review at native resolution.
5. **Later, optional:**
   - S.L.A.I.'s daily shop rotation (lineup ids still need item names);
   - sponsor tracks;
   - English briefings from the decoded MSG text;
   - the story layer.

## Status

### Phase 2, rules core (done, 2026-10-20)

Native C++ under `port/`, with no UI and no battle launch yet:

- `slai_rumble.hpp`, `slai_rumble_table.cpp` and `slai_rumble_world.cpp`
  load the data. `load_slai_rumble(roots)` reads `tables/slai_rumble.json`
  through the asset roots, so `mods/tables/` replaces it. It reads
  `data/world/slai_rumble.json` through the data roots, so mods overlay it.
  The world file holds:
  - city → day/night stage (Las Vegas night is `slai_day_lasvegas` with
    `night_lighting: lasvegas_night`);
  - maker names, and leg class → S.L.A.I. leg types;
  - the special-rule values and the status rule of each regulation;
  - the ladder ranges and the balance knobs.
- `slai_rumble_save.hpp/.cpp` hold `QuestSave::slai_rumble`: day, stage,
  city, finals cleared, and records by id (cleared plus beaten flags 1–14).
  Older saves load it empty.
- `slai_rumble_rules.hpp/.cpp` hold the rules:
  - `slai_scheduled_match` (0x257070, finals as data);
  - `slai_next_stage` (0x25A4E0, one step a battle);
  - `slai_entry_check` (0x259A40);
  - `slai_record_status` / `slai_record_beat` (0x2225A0 / 0x223ED0);
  - `slai_prize`, `slai_finish_match` (records, finals count, prize into the
    shared wallet up to `money_limit`, stage);
  - `advance_slai_rumble_day`, which `sleep_one_night` calls with
    `slai_days_in_cycle` (100, `data/ui/sleep.json`).
- Tests:
  - `slai_rumble_tests.cpp` (core suite): hand-made tables, the world data,
    the save block and sleep.
  - `slai_rumble_table_tests.cpp` (S.L.A.I. suite) checks the installed
    table against `port/tests/slai_rumble_expected.json`. That fixture holds
    campaign.py's `scheduled_match` for 10 sample days × every city, stage,
    slot and finals count, plus row spot checks and `next_stage` cases. It
    is written by `python -m phantomcrash_research.slai.rumble_fixture
    <iso> <out>`.

Traced while building it (capstone over the PAL executable):

- The ladder counters are 0x257440 (records 0x30–0x67), 0x257490
  (0x68–0x6E) and 0x2574E0 (0x1A–0x1C). The tutorial-done byte `+0x168` is
  record 0x18's cleared byte (0x18 × 15).
- `save+0xA14` is money (**High**): 0x24B470 clamps a battle's loss to it.
  0x259A40 only compares it with the entry requirement and takes nothing,
  so `charge_entry` defaults to false.
- The regulation's `+0x20` is a score target (**Medium**). 0x24B0B0 pays
  the prize when the player's result score (+0xC) reaches it, and a target
  read as −1 (0xFFFF) pays nothing. What the score counts is phase 3's
  question.
- The player's entry check has no always-eligible makers (only opponents
  have them, 0x222A60). A rental-only rumble refuses the player's own robot
  (case 0), so it needs a rental path.
- 0x223ED0's status table (0x371570) agrees with 0x2225A0's (0x371450) for
  regulations 0–26:
  - 5 flags for the tutorial;
  - 14 flags for the restriction and D–A rumbles;
  - open for the finals, Master, Free and Regional.

Left for phase 3: a `SlaiRobot` adapter from the ridden port robot. The
port's S.L.A.I. bodies keep their native type in `class`, not their maker,
and its parts carry no weapon class or aim kind. Also the score a battle
reports, and which opponents (record, flag) it beat.

## Gaps found so far

- **Opponent builds** are undecoded (phase 1b).
- **No Las Vegas night stage** was imported, although the disc has
  `LASVEGAS_NIGHT.MED` and the matches have Las Vegas night sets. The disc
  has no `NIGHT_LASVEGAS.BG`, so night Las Vegas is the day scene with night
  MED lighting. The importer reads no MED files at all (see
  SLAI_CAMPAIGN_RESEARCH.md, "Las Vegas night stage").
- **Unconfirmed meanings:** the 10 match weights, and what the score
  compared with the regulation's `+0x20` target counts (see Status).
- **Economy:** S.L.A.I.'s prizes (2,000–25,000) enter the same wallet as Phantom
  Crash money. The defaults stay the original's; the knobs sit in
  `slai_rumble.json`.
- **Current city:** the original allows rumbles only in the current city.
  How the player travels there is not yet traced.
