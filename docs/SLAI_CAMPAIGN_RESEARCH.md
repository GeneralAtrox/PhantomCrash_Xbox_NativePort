# S.L.A.I. single-player campaign research

Groundwork for a possible native "S.L.A.I. Rumble" mode in Club Wired. This is
research only; nothing here changes the port. The source is the PAL disc
SLES-52940 (`C:\Projects\SLAI\SLAI.iso`). All addresses are PAL virtual
addresses: the main ELF `SLES_529.40` (0x100000–0x43AD80, gp 0x4402F0) or
the `PC2PRG.STM` overlays (base 0x4D0C00; `towndvd.bin` is the town program).

The decoder is `src/phantomcrash_research/slai/campaign.py`, tested by
`tests/test_slai_campaign.py`. It reads through `SlaiDisc` and `ProgramData`
and refuses a release whose guarded code or tables differ.

Confidence labels:

- **High**: read from code and confirmed by the data, the tests or an
  independent tool (radare2 for the schedule geometry).
- **Medium**: one consistent code reading, not yet seen at run time.
- **Low**: an inference or a label only.

## Name provenance

Four kinds of names appear in this document. Keep them apart.

- **Shipped**: names embedded in the disc data.
  - Debug source filenames `talk.cpp`, `schedule.cpp`, `nextday.cpp`.
  - Event script names `main01t`, `bs01k`, `sponsor00k`, `rec_data01k`,
    `seigen01t`, `chip_out_t`, `news_t`.
  - Data paths `host0:data/talk/event/%s/%s.evt` and
    `host0:data/2d/schedule/sch01.ast`.
  - Disc filenames such as `UK_RUMBLETYPENAME.MSG` and `DAY_NEWYORK.BG`.
  - Battle keys such as `BossNYAD`.
- **Extractor** and **SDK**: none here (no new extractor names; this is a
  PS2 executable, so XbSymbolDatabase matches do not apply).
- **Researcher**: every other name is a researcher-assigned label. This
  covers stage, rota, regulation, match template, roster, story-event
  table, every field name and every EVT opcode name.

## Answer: what drives progression

The answer is **both**. A 100-day rota decides what is on offer each day,
and a 5-step stage ladder decides which rumbles the rota may offer. Both are
**High** confidence.

- **Days.** Day number `save+0xA10` runs 0..99 and wraps to 0 after 99.
  A separate counter `save+0x41DC` holds the total days elapsed, capped at
  9999; it feeds the "Day %03d" display. Both advance in `nextday.cpp` at
  town 0x4E8CF0. The sleep chamber triggers the advance (UK_WIREDMENUMSG
  says it advances 24 hours).
- **Cities.** Every day, each of the 7 cities hosts exactly one rumble,
  either in the day slot or in the night slot. The disc confirms this for
  all 100 days at stage 1.
- **Stage.** The stage byte `save+0xA21` (values 0..4) selects one of five
  match-id columns for each city and day. It advances only after battles,
  at 0x25A4E0:

| stage | advances when | rumbles the rota offers |
|---|---|---|
| 0 | tutorial done (f_257410: `save+0x168 == 1`) | match 0 only (tutorial rumble) |
| 1 | more than 55 of 56 ranker records cleared (f_257440) | Class Free/D/C/B/A rumbling + 16 restriction rumbles |
| 2 | all 7 regional records cleared (f_257490) | + Regional Match (head-to-head), one per city |
| 3 | all 3 finals cleared (f_2574E0 == 3), then f_25A7F0 | + 3rd/2nd-place head-to-head; finals injected (below) |
| 4 | — (post-game) | + Master Rank rumbling (`regulation 5`), 2 per city |

- **Finals injection.** While the stage is 3, the finals are not in the
  rota; code inserts them (0x257070, mirrored by `scheduled_match`).
  - The host city depends on how many finals are cleared:
    - none cleared: Tokyo hosts the 3rd-place match (match 2 in the day
      slot, 5 at night);
    - one cleared: London hosts the 2nd-place match (3 or 6);
    - two cleared: New York hosts the final (match 4, day only).
  - Only a slot that already holds a match changes.
  - In other cities, a scheduled 3rd- or 2nd-place match is replaced. With
    0 or 1 finals cleared, that city's Class Free rumble takes its place
    (table 0x334A80). With 2 cleared, the variant matches 0x85–0x88 take its
    place; these use save records 29 and 30. Their meaning is **Low**: they
    may be rematches.

A second gate is the story layer. Town events (below) appear by place, by
progress counts and by flags. No EVT opcode starts a battle (**Medium**).
Battles are reached from Club Wired's menu, not from scripts.

## Club Wired rota

Two tables define the rota (**High**).

- **Day table** `DAY_TABLE` 0x42D0F0, 100 × 24 bytes. The accessor is
  f_257350 (`day % 100`).
  - +0: five u16 values. Each comes from one block of 8 values (32–39,
    0–7, 16–23, 8–15, 24–31). Their purpose is unresolved (**Low**: perhaps
    shop stock or news picks).
  - +10: seven u16 pattern ids, one per city column. On the disc every
    pattern id is `column*100 + day`.
- **City-day table** `CITY_DAY_TABLE` 0x422CE0, 700 × 60 bytes.
  - The accessor is f_2572C0, and the row address is pattern × 60 +
    0x422CE0. radare2 independently confirms this.
  - f_2572E0 adds `stage*8 + slot*4`, so each row holds five
    (day, night) s32 match-id pairs. The value −1 means no match.
  - The 20 bytes after the five pairs are unresolved. They look like packed
    u16 data.
- **Cities.** City columns follow UK_FIELDINFOMSG order: 0 Tokyo (Shinjuku),
  1 Stuttgart, 2 New York, 3 London, 4 Hong Kong, 5 Cairo, 6 Las Vegas.
  - This order comes from the per-city match blocks. The 7 regional
    matches, each found in only one column, fix it (**High**).
  - The schedule screen draws the cities in byte order 0x43ACF0:
    2 6 0 4 5 1 3. That is the display order (**Medium**).
- **Schedule screen.** schedule.cpp, at town 0x57A7E0, shows 4 days × 7
  cities from that day onward. Each match cell displays the regulation's
  entry value (`+4`) and a name taken from the regulation name list
  (f_257400). An icon marks cleared matches (save record byte 0 == 1).
- **Today's rumbles.** f_24A0F0 builds today's rumble managers for the
  player's current city (`save+0x4769`): one for the day slot and one for
  the night slot.

## Regulations (rumble rules and rewards)

`REGULATION_TABLE` 0x4227E0 holds 32 rows of 36 bytes. Row *i* is named by
entry *i* of `UK_RUMBLETYPENAME.MSG` (shipped). Rows 0, 1 and 28–31 are
named "Dummy". The table was decoded through the player entry check at
0x259A40 and the opponent check at 0x222A60 (**High**).

| offset | field | values |
|---|---|---|
| +0x00 | id | equals the row index |
| +0x04 | entry requirement | 0/500/1000/1500/2000 classes, 2000 restriction, 4000 regional, 5000/7500/10000 finals, 10000 master; the player passes when `save+0xA14 >=` this (**Medium**: likely money, so an entry fee) |
| +0x08 | maker | 10 = any; otherwise the body maker must equal it (Carro 0, Proton 1, Hartman 2, KHT 3, Zwerg 4) |
| +0x0C..+0x18 | 4 leg classes | 11 any, 12 legged (leg types 0–2), 13 wheeled (3–6), 14 caterpillar/hover (7–10); −1 unused |
| +0x1C | special | −1 none, 0 rental only, 1 no optical camouflage, 2 shoulder weapons only (arm slots empty), 3 arm weapons only, 4 close-range (weapon class 2), 5 lock-on (+0x18 ∈ {1,9}), 6 grenade (+0x134 ∈ {3,15}), 7 E-Chip only (chip 0/1) |
| +0x20 | u16 param | 1 head-to-head; 20/25/30 for rumbles. Copied into the rumble manager; meaning unresolved (**Low**: perhaps a time limit or a score target) |
| +0x22 | u16 prize | 2000 Free … 4000 A, 3500 restriction, 15000 regional, 18000/20000/25000 finals, 10000 master (**Medium**) |

The prize pattern for each class is: Free 0 → 2000, D 500 → 2500,
C 1000 → 3000, B 1500 → 3500, A 2000 → 4000.

Sponsors: winning a maker-restricted rumble (types 6–10) opens that maker's
sponsor track (`save+0x4269`, `+0x426B`; 0x25A4E0). Some leg-class and
weapon-restricted wins also open a track, chosen by f_25A7B0 (**Medium**).

## Match templates

`MATCH_TABLE` 0x41ECE0 holds 157 rows of 56 bytes, read by the rumble
constructor 0x222070 (**High** for the layout, **Medium** for the meanings).

| offset | field | notes |
|---|---|---|
| +0x00 | regulation | index into the regulation table |
| +0x02 | night | 0 day, 1 night (rows come in day/night pairs) |
| +0x04/+0x06 | robots min/max | rumbling 3..6, head-to-head 1..2 (Cairo night regional, row 125, 1..4); capped at 6 |
| +0x08 | level | Free 2, D 1, C 2, B 3, A 4, master 5, restriction 2; −1 head-to-head; 139 on "No optical camouflage" (unresolved) |
| +0x0A | save record | 15-byte record id (below) |
| +0x0C | save record b | second id, also used to pick opponents |
| +0x0E | pad | 0 |
| +0x10 | 10 floats | per-match weights (**Low**: perhaps the AI or part mix) |

Layout of the 157 rows:

- 0 and 1: the tutorial rumbles.
- 2–7: 3rd-place, 2nd-place and final matches, each as a day/night pair.
- 8–24 (day) and 25–41 (night): one Master Rank row, then the 16
  restriction rumbles.
- 42–125: 7 city blocks of 12 rows, ordered NY, LV, London, Tokyo, HK,
  Stuttgart, Cairo. Each block holds Free, D, C, B, A and Regional, first
  the day set, then the night set.
- 126–132 and 137–144: "Dummy" regulations 28–31. 7 + 7 of these rows may
  be the battle-school or exam battles (`BossEXAM`, `QU01`–`QU07`; **Low**).
- 133–136: the 3rd/2nd-place variants 0x85–0x88.
- 145–156: the other 12 Master Rank rows.

**Status.** 0x223ED0 sets a rumble's status:

- 3 = cleared (record byte 0 == 1);
- 2 = every opponent flag set. This needs 14 flags, or 5 for the tutorial
  type.

**Challengers.** 0x2240E0 can add a roster challenger. The roster table
`0x420F40` has rows of 30 bytes (14 u16 opponent ids) indexed by save record.
f_256CC0 resolves an id to a robot or ranker. The robot builds behind those
ids are **not decoded yet**. They are the main missing piece for opponents.

## Save layout touched by the campaign (save object 0x4B90 bytes)

| where | meaning | confidence |
|---|---|---|
| `id*15` | 15-byte rumble records: byte 0 cleared, bytes 1..14 opponent beaten flags | High |
| ids 0x18–0x19 | tutorial; 0x1A–0x1C finals (3rd/2nd/final); 0x1D–0x1E final variants | High |
| ids 0x20–0x2F | restriction rumbles (16) | High |
| ids 0x30–0x67 | 56 city × class ranker records; ids 0x68–0x6E 7 regional; higher = master | High |
| `+0xA10` / `+0x41DC` | rota day 0..99 / elapsed days (cap 9999) | High |
| `+0xA14` | value the entry requirement is compared with (likely money) | Medium |
| `+0xA20` | byte set by town 0x4EF9C0; story events compare it against 0..8 (9 = any) | Low |
| `+0xA21` | stage 0..4 | High |
| `+0x168` | tutorial-done byte | Medium |
| `+0x426F` | 256-bit event flag field (test 0x204E50, set 0x204F20, clear 0x204FC0) | High |
| `+0x4769` | current city | Medium |

f_24B5A0 builds a completion score from these records; event conditions
test it against 999.

## Story event table

`STORY_EVENT_TABLE` 0x335E80 holds 154 rows of 28 bytes; the getter 0x26A920
refuses indices of 0x9A or more. The selector is 0x26A420(kind, place,
previous place), called from 0x262980 (**High** for the layout, **Medium**
for the fields).

The rows are in story order:

- `chip_out_t`;
- 100 main, battle-school, news and boss rows (`main01t` … `main73k`);
- `sponsor00k`–`29k`;
- `rec_data*`;
- `seigen*`.

The last 8 rows are not scripts: six point at 8-byte sdata records
(0x438D98–0x438DC0) and two are null. The decoder names 146 rows.

| offset | field |
|---|---|
| +0x00 | script name pointer (selects `<name>.evt`) |
| +0x04 | optional condition function, called with +0x12 |
| +0x08 | trigger kind (0: 86 rows, 1: 58, 4: 6, 2: 2, 3: 2) |
| +0x0A | flags: 1 talk background resources, 2 skip load, 8 reset, 0x40 freeze.pig |
| +0x0C/+0x0D | place / sub-place (place < 0x32 must match; 0x1C also matches 0x1D; sub > 0x31 any) |
| +0x0E | must equal `save+0xA20` when 0..8 |
| +0x10 | progress threshold: 1..0x38 ranker count, 0x39..0x3F regional count, ≥0x40 finals count |
| +0x12 | condition argument |
| +0x14 / +0x16 / +0x18 | required flag / blocking flag / completion flag (`save+0x426F`) |
| +0x1A | unresolved |

The condition functions sit at 0x26A990–0x26AF40. They test:

- tutorial state;
- money thresholds on `save+0x4438` (>99, >0x23C, >999, >4999);
- the completion score;
- sponsor levels at `save+0x44D5`, `+0x44D6` and `+0x44DA`, with flags
  0x6B/0x6F/0x73/0x77/0x7B.

Setting flags 0x16, 0x23, 0x28, 0x46, 0x4E or 0x61 calls f_256E70, which
unlocks content (**Medium**).

## MSG text format (**High**, round-trip tested)

- **Layout.** `u32 count`, `u32 offsets[count]` (file offsets), then u16
  glyph strings, each ending in 0xFFFF.
- **Glyphs.** Glyphs 0x00–0x5E are ASCII − 0x20. Higher glyphs are a
  Japanese font plus accented letters and symbols; German, for example, uses
  0xA0 for an umlaut. Some UK files keep leftover Japanese.
- **Control codes:**
  - 0xFFFE: newline;
  - 0xFFFA / 0xFFFB / 0xFFFC: name placeholders (player, partner, alias);
  - 0xFF91–0xFF9B: open style 0..10, which is colour or emphasis;
  - 0xFF9C: close style.
  - Glyphs 0x81, 0x18E, 0x190 and 0x195 are button icons.
- **Decoded sample**, entry 0 of `UK_SCHEDULE_SYSMSG.MSG`:
  `Highlight match and press {0081} button for info.`
- **In the decoder.** `glyph_text` renders unknown glyphs as `{xxxx}`, and
  `text_glyphs` / `write_msg` encode text back.

## EVT script format and interpreter

**Format** (**High**: all 143 English scripts decode exactly to their string
table):

- `u32 n`, then `u32 offsets[n+1]`. These are file offsets that the loader
  relocates.
- offsets[0] is the bytecode start; offsets[1..n] are strings in the MSG
  glyph encoding.
- **Bytecode** is a stream of u16 opcodes, each followed by a fixed number
  of u16 arguments. Opcode 0 ends the script.
- **String arguments** are 0-based. String 0 is `???` in every script
  sampled (**Low**: the unknown-speaker nameplate).

**Packages and loader:**

- `PC2.STM` holds five language packages: 0x1B4F000 Japanese, 0x1BA6000
  German, 0x1BED000 French, and 0x1C31000 / 0x1C75800 English (nearly
  identical). Each holds 143 scripts plus `FILELIST.LST`.
- The loader is town 0x4EB1B0, using the path format at 0x5977F0. f_4D4640
  picks the language directory.

**Interpreter** (town overlay, `talk.cpp`):

- The script pointer is 0x43B71C, the VM state 0x43B714 and the resources
  0x43B718. The per-frame stepper is 0x4EB560.
- The opcode table is in the ELF at 0x336F70: 74 opcodes × 24 bytes, each
  `{exec fn, poll fn, u32 argc, pad, u64 blocking mask}`.
  `evt_opcode_argc` checks the decoder's embedded argc list against this
  table.
- **Branching.** When VM[0x1DD] ≥ 0 and differs from VM[0x1DC], every
  opcode except 0x35 (case) and 0x3D (end case) is skipped. VM[0x1DC] comes
  from f_4EA860, which is not decoded (**Medium**).

Coarse opcode groups (all researcher labels, **Medium** unless noted):

| opcodes | group |
|---|---|
| 01–08 | scene/frame open animations |
| 09/0A, 0D/0E, 12, 1D | UI panel show/hide |
| 0B (4) / 0C / 31 | open talk window, close window, select active window |
| 21 / 36 | show dialogue string / secondary string (**High**) |
| 20, 2E/2F/30, 1F | wait for button, sync, wait N frames |
| 34, 37/38, 39, 3A/3B, 3C | window style, text position, text parameter, margins, clear |
| 35 (2) / 3D | case / end case for choice branches |
| 23/24/25/27/28/29 | choice and menu prompts (f_4EA6C0) |
| 2D | day/week display |
| 16/1A, 17 (3), 18/19, 1B/1C | portrait loading, portrait with expression, slot operations |
| 10 / 11 / 0F (4) | background image / event image / unresolved |
| 14 / 15 | fade in / out (duration × 2) |
| 42 (2) / 43, 44 / 45, 2A | BGM, sound effect play/stop, ambient loop |
| 2B | tutorial setup (MAIN01T/03T only) |
| 3E/3F/40, 41, 47/48, 49, 46 | VM flag bits and state writes; 46 is a no-op |
| 13 | unresolved (f_4E3B00) |

Scripts present dialogue only. Story progress comes from the event table's
completion flags and from town code; no opcode sets a story flag or starts
a battle (**Medium**).

## Guarded addresses

The decoder checks these before trusting the data:

- code words at 0x2572C0 (city-day row geometry);
- code words at 0x257350 (`% 100`, base 0x42D0F0);
- code words at 0x26A920 (event bound 0x9A, base 0x335E80);
- the argc column of the EVT opcode table;
- regulation ids equal to the row index;
- every match row having a valid regulation and day/night value.

Other key addresses (unguarded):

- rumble manager in the ELF: 0x222070 constructor, 0x2225A0 opponent list,
  0x222A60 opponent eligibility, 0x222F70 completion test, 0x223ED0 status,
  0x2240E0 challenger;
- player entry check 0x259A40; stage advance 0x25A4E0; rota lookup
  0x257070;
- town code: schedule screen 0x57A7E0, next day 0x4E8CF0.

## What a native S.L.A.I. Rumble mode needs

**A. Battles only** (Club Wired rota, no story; recommended first). This is
mostly data plus one menu.

1. Export the regulations, match templates and rota to TOML with
   `campaign.py`. The data is all decoded except the 5 day-header values
   and the 20-byte row tail. **Small.**
2. Decode the roster table 0x420F40 and the robot or ranker definitions
   behind f_256CC0, so every opponent has a real build. This is the largest
   research gap. It needs the opponent part ids mapped to the imported
   S.L.A.I. parts. **Medium**: 1–2 sessions.
3. Map arenas: the 7 cities to the imported stages (`DAY_<CITY>.BG` and
   their night counterparts), with day/night lighting for each match.
   Confirm which battle key (`Boss<city><rank><D/N/R>`) each match loads.
   **Small to medium.**
4. Port the rules:
   - the entry check (requirement, maker, leg class, special);
   - the robot count from match min/max;
   - prize payout;
   - the 15-byte clear/opponent records;
   - the stage ladder and finals injection (already mirrored by
     `scheduled_match` / `next_stage`).
   **Medium**: 1–2 sessions.
5. A Club Wired schedule screen showing 4 days × 7 cities, a city picker,
   day/night entry, and sleep to advance the day. The UI is authored at
   640x480 with sharp text. **Medium**: 1–2 sessions.

Estimate: about 5–8 focused sessions. The opponent builds (step 2) are the
critical path. Unresolved meanings, such as the `+0x20` parameter and the
match weights, need runtime checks in PCSX2.

**B. Full story.** Everything in A, plus:

- **Script renderer.** A talk-scene renderer for the 74 opcodes: windows,
  portraits with expressions, backgrounds, choices, fades and BGM. About
  20 opcodes are still coarse or unknown, and f_4EA860 (the case selector)
  is undecoded.
- **Trigger system.** The 154-row trigger system with its ~30 hand-written
  condition functions, the place/sub-place town navigation, and the
  256-bit flag set.
- **Town systems.** Battle school, news, sponsors, shops, records and the
  ending path (f_25A7F0).
- **Text.** Text in three European languages, plus the leftover Japanese
  glyphs, which need the SLAI font atlas.

Estimate: roughly 3–4 times the work of A. The town and talk systems have no
counterpart in Phantom Crash's own UI, and every one of them needs careful
visual review. Recommendation: build A, and keep B as optional data-driven
content later. The EVT and MSG decoders here make the scripts readable now,
for writing English story cards or briefings without the full interpreter.

## Open questions (most need runtime checks in PCSX2)

- What do the five day-header values at day table +0 and the 20-byte tail of
  each city-day row mean?
- Is `+0xA14` money? What do the regulation's `+0x20` parameter and the 10
  match weights do?
- What builds sit behind the roster ids (f_256CC0)? This needs ranker and
  robot data decoding.
- What are the semantics of `save+0xA20`, of f_4EA860, of EVT opcodes 0F,
  13, 1B and 1C, and of story trigger kinds 0–4?
