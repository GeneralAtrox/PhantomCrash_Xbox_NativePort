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

A new `slai_rumble` block in `QuestSave` (and `quest_json.cpp`). Base game and
S.L.A.I. share one save, so the block is always read and written, and stays
untouched while S.L.A.I. is off:

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

Decided 2026-10-21 from mock-ups; the reference picture is
`schedule_toggle_slai_cities.png` (one row per city, CLOSED where a city does
not run that day).

- **Schedule:** the Phantom Crash schedule is unchanged apart from a new
  button between SKIP TO DAY and EXIT that toggles to the S.L.A.I. page and
  back (labelled with the other calendar's name). The S.L.A.I. page has one
  row per city in the original's display order (2 6 0 4 5 1 3), five day
  columns, and in each cell the regulation for the player's current stage,
  with DAY or NIGHT; cities not running that day read CLOSED. Cleared marks
  as on the Phantom Crash page. SKIP TO DAY works from either page.
- **Club Wired:** a new item, "Rumble S.L.A.I.", shown only with S.L.A.I.
  content. The wheel is authored for six items modulo six, so it must take
  the item count from data. The label is sharp text on an existing wedge
  image; the original pictures stay untouched.
- **Rumble S.L.A.I.:** today's three cities and their matches; the player
  picks one. It reuses the contest-info popup for rules and prize.

### Days

Decided 2026-10-21: S.L.A.I. runs on Phantom Crash's 60-day calendar, three
cities a day. There is no separate S.L.A.I. day counter; phase 2's
`slai_rumble.day` becomes the Phantom Crash day.

- **Rota rules.** Each day opens three of the seven cities. No city opens on
  two days in a row, so a day picks three of the four cities closed the day
  before. Within a day no regulation repeats at any stage (never two CLASS D,
  never two REGIONAL MATCH), so every day is a mix.
- **Where matches come from.** A city's match on Phantom Crash day `p` is that
  city's S.L.A.I. line-up from one S.L.A.I. day near `p*100/60` (within two
  days of the day range `p` maps to). The borrowing is needed because the
  original themes most days: on its day 2 every city runs CLASS D.
- **Coverage.** 180 city-days cannot hold S.L.A.I.'s 700, but almost every
  distinct match fits: a search found a rota with 99/102 stage-1, 115/116
  stage-2, 119/120 stage-3 and 132/134 stage-4 matches. Generation should push
  this towards 100% (a wider window or an optimiser).
- **Data.** The rota is port-authored data, `data/world/slai_rota.json`: per
  day, three entries of city and source S.L.A.I. day (no disc content). A
  research command generates it from the extracted `slai_rumble.json`; tests
  check the rules and report coverage. Mods can hand-edit or replace it.
- Finals, the stage ladder and records stay S.L.A.I.'s. Shop lineups (later)
  follow the source S.L.A.I. day of the first open city.

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

### Rota, schedule page and Rumble S.L.A.I. (done, 2026-10-21)

- `data/world/slai_rota.json`:
  - three cities a day for 60 days, none two days running;
  - matches borrowed from within two S.L.A.I. days;
  - covers all 472 day and night matches.
- The schedule's S.L.A.I. page sits behind a toggle between SKIP TO DAY and
  EXIT (`data/ui/schedule.json` "slai"). Removing the entry removes the
  toggle.
- Rumble S.L.A.I. (`data/ui/slai_rumble.json`, `slai_rumble_hub.hpp`):
  - Club Wired's item `requires` S.L.A.I.; without S.L.A.I. the host drops
    it.
  - The screen lists today's cities; entry checks money and a ready ridden
    robot only.
- The battle is a stand-in (`desktop_screens_slai.cpp`):
  - the ridden robot, fitted as for TEST DRIVE, fights on the city's
    imported stage;
  - the opponents are training drones, one fewer than the match's most
    robots;
  - the score is the shootdowns. The match's leader is beaten when the
    player is standing with at least that many shootdowns.
- Still to do:
  - opponent rosters (phase 1b; done below);
  - the `SlaiRobot` adapter (maker, legs, special rules);
  - Las Vegas night lighting;
  - the RUMBLING lettering on the wedge art the Club Wired item reuses.

### Armed opponents from the original's rosters (done, 2026-10-26)

- Registration writes an `"opponents"` block into `tables/slai_rumble.json`
  (`slai/opponent_table.py`, table version 2). It holds:
  - the 210 rosters, the record 0x7D roster swap, the substitutions and the
    leader rules;
  - the 1,805 opponents they name, each with its robot already on the port's
    imported parts (garage robot format) and what the opponent check reads.
- `slai_lineup.hpp` reimplements the line-up (`opponent_rules.py` mirrors
  it):
  - the opponent check (0x222A60), through the player's entry check, with
    rental-only binding only the player;
  - the candidate lists (0x2225A0);
  - the per-place draw (0x223420): places 1–3 from the rosters, 4–5 from
    the pool.
  - `port/tests/slai_lineup_expected.json` (`slai/lineup_fixture.py`) checks
    48 match and save cases and 144 scripted draws against the disc.
- The battle (`desktop_screens_slai.cpp`, `slai_battle_lineup.hpp`,
  `desktop_training_lineup.cpp`) fields the drawn opponents:
  - each on its own robot, every robot its own team;
  - fighting with the live RUMBLING's AI (`rumbling.json` live.ai,
    `robot_ai.json`), at its level as the AI's ability.
  - A place emptied by a shootdown draws again after `arrival_seconds`. The
    draw re-reads the records as this battle filled them, as 0x223ED0
    re-checks after each defeat. This is a port reading: the 20–30
    shootdown targets need it.
- Results:
  - each roster opponent the player shot down sets its record flag;
  - the leader's clears the record;
  - the score is still the shootdowns.
  - Without the opponents block (an older table), the training-drone
    stand-in remains.
- Port choices, in `data/world/slai_rumble.json` "opponents":
  - AI chips by level (Phantom Crash's enemy chips 95…91), since the CPU rows
    are undecoded;
  - the AI profile, 4 for every behaviour value;
  - no story flags set;
  - the leader's session check passes; regulation 30's counter does not;
  - default paint, since the 16-region paint is undecoded.

## Gaps found so far

- **Opponent builds** are decoded and fielded (see above). Still undecoded:
  the CPU row, the behaviour value, the 16-region paint, the emblem, and
  the story flags behind the substitutions.
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
