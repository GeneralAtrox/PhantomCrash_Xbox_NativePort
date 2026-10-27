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

Shop stock also rotates with the rota day (**High** for the reads,
**Medium** for which shop): see the day table and city-day rows below.

A second gate is the story layer. Town events (below) appear by place, by
progress counts and by flags. No EVT opcode starts a battle (**Medium**).
Battles are reached from Club Wired's menu, not from scripts.

## Club Wired rota

Two tables define the rota (**High**).

- **Day table** `DAY_TABLE` 0x42D0F0, 100 × 24 bytes. The accessor is
  f_257350 (`day % 100`).
  - +0: five u16 values. Each comes from one block of 8 values (32–39,
    0–7, 16–23, 8–15, 24–31). They are daily shop lineups: shopimp.cpp
    (0x54BA40, via 0x547960) picks the value for shop id 0x43AB48 and reads 3
    items from the lineup table 0x333A10 (6-byte rows; items 0x9C bytes at
    0x382390).
  - +10: seven u16 pattern ids, one per city column. On the disc every
    pattern id is `column*100 + day`.
- **City-day table** `CITY_DAY_TABLE` 0x422CE0, 700 × 60 bytes.
  - The accessor is f_2572C0, and the row address is pattern × 60 +
    0x422CE0. radare2 independently confirms this.
  - f_2572E0 adds `stage*8 + slot*4`, so each row holds five
    (day, night) s32 match-id pairs. The value −1 means no match.
  - +0x28..+0x3B are per-city daily shop lineups (u16 lineup ids):
    +0x28 for chipshop.cpp and chipshop_select.cpp (up to 10 chips, 0x51C2E0);
    +0x2A.. per category and +0x38 for the module-shop area (0x5372D0,
    0x537480: 7 categories × 14 items). When flag 0x43B91C is set, that shop
    instead uses a 10-day cycle per city: `day/10*10 + offset[city] − 1`
    (offsets 0x43A908 = 7 8 2 6 1 10 4), then +0x3A into 0x333B00 (16-byte
    rows). What sets that flag is unresolved.
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
| `+0xA20` | current city or field (the shops pass it as the rota city column); set by town 0x4EF9C0; story events compare it against 0..8 (9 = any) | Medium |
| `+0xA21` | stage 0..4 | High |
| `+0x168` | tutorial-done byte | Medium |
| `+0x426F` | 256-bit event flag field (test 0x204E50, set 0x204F20, clear 0x204FC0) | High |
| `+0x4769` | current city | Medium |
| `+0x41CC` | enemies shot down in all battles (64-bit) | High |
| `+0x4269` / `+0x426A` | sponsor / previous sponsor: 0 Hartman, 1 Proton, 2 Carro, 3 KHT, 4 Zwerg, 5 none | High |
| `+0x426B` | sponsors had, a bit each | High |
| `+0x426C` | battles shot down in a row | High |
| `+0x426E` | why the last sponsor left: 1 body, 2 shot down, 3 score | High |

Offsets are from the save object (global + 0x26C); the condition functions
address the same fields from the global (`+0x4438` is `+0x41CC`).

**Completion score (0x24B5A0, High).** Each part's count, capped, scales to
its points: records 0x30.. (56 → 3920), 0x68.. (7 → 700), 0x1A.. (3 → 600),
0x20.. (16 → 800), the collections at save + 0x4220 (238 → 2500), decals
(229 → 500), music (90 → 480) and the sponsors-had bits (5 → 500). The sum
over 10, rounded half up, is 0..1000; event conditions want more than 999.

**Battle result (global + 0x489C, High for the layout).** [1] the player
survived, [2] excluded from the sponsor rules (who sets it is not traced),
+0xC the score (the player's battle struct + 0x24, not traced). After a
battle 0x24A9E0 keeps the previous sponsor, then unless excluded: outside
the head-to-head matches (2..7, 0x2F, 0x35, ... 0x7D, 0x85..0x88) a score
under 10 dismisses the sponsor (reason 3) and being shot down raises the
run, two dismissing it (reason 2); then a previous sponsor whose brands
(Hartman {2}, Proton {5, 1}, Carro {7, 0}, KHT {6, 3}, Zwerg {4}) lack the
body's brand (body + 2) dismisses it (reason 1). 0x25A4E0 then offers a
sponsor on a score over 14: by regulation (6 Proton, 7 KHT, 8 Carro, 9
Zwerg, 10 Hartman) or, for regulations 11..14 and 16..21, by the body's
brand (0, 7 Carro; 1, 5 Proton; 2 Hartman; 3, 6 KHT; 4 Zwerg). An offer
sets the sponsor and its bit and resets the run.

**Nights (0x4E8CF0, 0x4E8D70, High).** A night adds a day (cap 9999) and,
for the pairs at 0x5C4160, sets the second flag when the first is set:
(27, 28), (37, 38), (45, 47), (66, 67), (75, 76), (107, 108), (111, 112),
(115, 116), (119, 120), (123, 124).

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

- tutorial state, and the days (`save+0x41DC`: first day or later);
- enemies shot down (`save+0x41CC`, read from the global as `+0x4438`):
  more than 99, 0x23C, 999 or 4999; a milestone whose successor's flag
  (0x83, 0x84, 0x85) is set marks its own (0x82, 0x83, 0x84) instead;
- the last battle survived or lost;
- the completion score and the three collections;
- the town spot entered being the junk module shop (spot 0x0F, place map
  0x4DF8E0);
- the sponsor fields (global `+0x44D5`, `+0x44D6`, `+0x44DA` are save
  `+0x4269`, `+0x426A`, `+0x426E`): each needs the sponsor to have just
  changed; an offer is open or due by the flags 0x6B/0x6F/0x73/0x77/0x7B,
  a level names the dismissal reason.

When a move plays no row, 0x26A7D0 clears the completion flags of the rows
flagged 8 (reset), so they may play again.

Setting flags 0x16, 0x23, 0x28, 0x46, 0x4E or 0x61 calls f_256E70, which
unlocks content (**Medium**).

**Selection as the port reads it** (`slai_story.cpp`). A row plays when its
kind and places match, its city (0..8) equals `save+0xA20` or it names none,
its required flag is set (or -1), and its blocking and completion flags are
clear. The threshold compares one progress count: won ranker records
(0x30..0x67), plus regional (0x68..0x6E) from 0x39, plus finals from 0x40
(**Medium**: the ranges are static; treating them as one running count is
an assumption that holds when records are won in ladder order). The
tutorial conditions read record 0x18's cleared byte (0x257410) and the
days; the other conditions read the story's counts as above
(`slai_story_rules.cpp`). Places are S.L.A.I.'s names, mapped to port screens in
`data/story/slai_talk.json` (docs/SLAI_STORY_PLAN.md).

## Talk scene assets

`register-slai-talk` (`slai/talk_register.py`, also run by the S.L.A.I.
install) converts the talk scenes into the S.L.A.I. asset root
(`artifacts/slai/assets`):

- **Pictures.** The talk archives' PIG pictures as PNG under
  `talk/<folder>/` (portraits and expressions, `talk/common` window art
  TAL01–04, `talk/de000` the Knocker, `talk/news/<n>` news and event
  pictures) and `2d/s_chara`, `2d/s_chip` icons. Folder and file names are
  the disc's own (shipped names); the PNGs are the original pictures,
  unchanged.
- **Animations.** Each ANB bank as JSON beside its picture (`slai/anb.py`
  reads and writes the format; layout in its docstring, **High**,
  round-trip tested): cells with UVs, pivots, two scale/rotation stages,
  blend and RGBA; frames as cell lists; anims as {frame, tween, loop, jump,
  end} commands.
- **Font.** FONT01..05 as `font/font01..05.png` and `font/glyphs.json`
  (`slai/talk_font.py`): 896 glyphs a page in 32×28 cells of 16×18, with
  the {left, width} metrics from 0x36E2E0 and the four fixed ranges
  (0x206B20). FONT_A01/02 (debug) are pictures only.
- **Scripts.** Every EVT script per language as
  `talk/event/<uk|us|de|fr>/<name>.json`: `{"code": [[op name, args...]],
  "strings": [...]}` with text in readable tokens (`{player}`,
  `{style:n}`...`{/style}`, `{hhhh}` for glyphs without a character;
  `glyph_text` / `talk_glyphs` invert each other). The Japanese package is
  not exported.
- **Table.** `tables/slai_talk.json` (`slai/talk_tables.py`): the opcode
  table (name, argc, polls, `waits_for`), characters (folder, icon, name,
  chip icon, expressions), the story event rows, news pictures, the window,
  prompt and style colours (0x337660, 0x337670), panel sprite places
  (0x337C20), the login curve (0x3376A0) and text, the Knocker texture mask
  (0x1807FF), the voice meter sets (0x5977D0) and the sound cues.
- **Sounds.** The talk cues as `slai/assets/sound/cue_NNN.wav`
  (deterministic extractor names); BGM streams are the S.L.A.I. music
  conversion's `sound/slai/bgm_NN.ogg`.

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
- **Branching** (**High**, static). `case` (0x35) sets VM[0x1DD] to its
  first argument and `end_case` (0x3D) resets it to -1 (**Medium**: the
  reset is read from the exec function, not seen at runtime). While
  VM[0x1DD] ≥ 0 and differs from VM[0x1DC], every opcode except 0x35 and
  0x3D is skipped. A `case` that matches also restyles the window (style 2
  for selector 0, else 3).
- **Case selector f_4EA860** (**High**, static). VM[0x1DC] is the partner
  chip's type: `save+0x9FC` (current hangar, s16) < 0 gives 0; otherwise the
  hangar's robot record (0x25A870, stride 0x68) names a part in 0x25A850
  (stride 8, +4 item id), whose item record (0x24D440: 0x3DF340 + id × 0x22C,
  id clamped 0..0x1C3) holds a u16 chip type at +6. Types 2..9 give
  selectors 1..8; anything else 0. The port has no CPU types; it gives
  each Phantom Crash chatter personality a selector (docs/SLAI_STORY_PLAN.md,
  "The partner chip").
- **Partner ops** (**High**, static). With a selector, partner_in
  (0x270790) skips its argument and plays TAL01 anim 0x29 on object 15
  (VM+0xA0), holding, clearing flags 0x81; partner_out (0x2708D0) plays 0x2A
  and hides object 14, the nameplate; their polls (0x270850, 0x2709A0) wait
  for the anim's end, the second then hiding object 15. knocker_only_pose
  (0x270AD0) does nothing. partner_path (0x271980) takes a path and a
  count: count 0 repeats for ever, and path 2's count is in frames
  ((n + 6) / 7 passes). Its poll (0x271A40) places object 15 at (256 + step,
  224) on a horizontal path, else (256, 224 - step), object 14 with it while
  it plays anim 0x2B (else at (256, 224)), one step a frame; at the path's
  end it finishes when the count runs out. partner_path_stop sets the count
  to 1. The paths at 0x42DED0 (pointer, length, horizontal) are 25, 10 and
  7 steps. Outside talk mode the ops drive the town HUD's chip window
  (0x4E7350 and on) instead. TAL01's texture 4 there is the chip picture.

Opcode names are researcher labels (`EVT_OPCODE_NAMES` in
`slai/campaign.py`); the wait masks come from the opcode table. **High**
where the exec and poll functions were read, **Medium** for meanings drawn
from them:

| opcodes | names | what they do |
|---|---|---|
| 01–07 | backdrop_in, haven_open(_2/_3), haven_close, rings_out(_2) | H.A.V.E.N. backdrop, its opening objects, petal and ring sprites and their fades (see below) |
| 08 | login_screen | FREEZE.PIG reboot screen; percentage follows the (frame, value) curve at 0x3376A0 |
| 09/0A, 0D/0E | panel_open/close, news_open/close | portrait panel and HPC news frame objects |
| 0B/0C/31 | open_window, close_window, select_window | talk windows 0..3 |
| 0F, 16, 1A, 22, 26, 30 | loads and their waits | instant in the port |
| 10 | background | 0 none, 1 news frame, n>1 news picture n (256×256 at 256,169); alpha ±0x20 a frame, SE 0x2C |
| 11/12 | event_portrait, event_portrait_close | the chip window and its shade (see below) |
| 18/19/1E | badge, badge_remove, badge_focus | joined characters' faces on H.A.V.E.N.'s petals (see below) |
| 1B/1C/1D | event_image, event_image_open, event_image_close | the character window: open, close, keep the chip window as it (see below) |
| 13 | town_hud | the town HUD's state (0x26FFF0: state 1 keeps town text at y 409, otherwise 379) |
| 14/15 | fade_in/out | from/to black over 2n frames |
| 17 | portrait | character, expression, chip icon (-1: the character's own) |
| 1F/20/21/2E/2F | wait_frames, wait_button, say, sync, sync_portrait | waits are the table's blocking masks |
| 23/24/25/29 | knocker_in/out/pose, knocker_only_pose | the Knocker (see below) |
| 27/28/2D/49 | partner_in/out, partner_path(_stop) | the partner chip (see "Partner ops"); with case selector 0 (no partner chip) they are the Knocker's ops and the path ops do nothing |
| 2A | ambient_loop | a looped SE and three TAL01 objects (see below) |
| 2B | glitch | the town break-up and text rate (see below) |
| 2C | panel_sprites | the panel's seven icons at 0x337C20, alpha ±0x20 over 4 frames |
| 34/37/38/39/3A/3B/3C | window_style, glyph_size, ruby_size, text_speed, text_cursor, text_offset, clear_text | text layout |
| 35/3D | case, end_case | branches (above) |
| 3E | text_flag_1 (0x270B70) | arg 0 sets VM[6] bit 1: no blinking |
| 3F | line_feed (0x270C10) | arg 0 sets bit 4: the next say continues the line |
| 40 | allow_skip (0x270BC0) | arg 0 sets bit 2: typing cannot be skipped |
| 41 | talk_sounds | VM[0x1AD]: the talk animation and voice meter set |
| 42/43, 44/45 | bgm, bgm_stop, se, se_stop | 0x203470(id, 0, 0x76) clamps: the bgm id is the stream index (`sound/slai/bgm_NN.ogg`) |
| 32, 33, 47, 48 | unload_character, unload_all, unload_knocker(_2) | free loaded files; nothing to do in the port |
| 36, 46 | ruby, nop | |

**Talk scene player details** (**High** unless noted; all static, from the
exec/poll functions named):

- **Badges** (FaceWork, TALK_JOIN_CHARA_MAX = 4; slots at VM+0x490, 12
  bytes: character, name text, face sprite; shown bits VM+0x4C0). `badge`
  (0x26EED0; character, slot, flag): the same character only renames the
  slot (flag ≠ 0: "???" from 0x4390E8). Otherwise slot (x,y) from 0x337A30
  = (41,191) (122,159) (177,76) (187,−11): frame object VM+0xB0+4·slot plays
  TAL01 anim 0xB at (x+256, 224−y); the name (16×17) at (x+0xD0, 224−y) in
  0x805E22 from alpha 0; the face (the icon picture, uv 4,0 56×64) centred
  at (x+0xEA, 0x10A−y), grey 0x40, alpha 0; sprite 20+slot shown. Poll
  0x26F1B0: faces +0x20 a frame; names +0x20 once their frame has ended.
  `badge_remove` (0x26F2F0): frame anim 0xC, name deleted; poll 0x26F3D0
  fades the face −0x20, then hides sprite 20+slot and empties the slot.
  `badge_focus` (0x270DB0) and 0x4EF850 set VM+0x4C1; poll 0x270DE0: the
  focused slot's sprite 20+i fades out, 16+i fades in, 12+i shows; others
  the reverse; face grey = alpha(16+i)/2 + 0x40. Sprites 12–23 (0x4EC4D0)
  are TAL02 128×160 pieces on petals 0–3 (12–19 u=256, 20–23 u=128), start
  hidden at alpha 0x80; 12–15 have +0x74 = 1, which 0x1CD300 draws with GS
  ALPHA 0x48 (Cs·As + Cd: additive).
- **Chip and character windows.** `event_portrait` (0x26DC70): the
  character's icon becomes TAL01's fifth texture; a new object
  (pChipWindowChip, VM+0x6A0) plays anim 0x1D at (256,224); object 9 plays
  0x1B at full colour; focus the character; SE 0x2A. `event_portrait_close`
  (0x26DE50): anim 0x1E, object 9 anim 0x1C, nameplate hidden, focus the
  portrait's character, SE 0x2B. `event_image` (0x26F7B0) opens the
  character window (pCharaWindowChip, VM+0x69C) with anim 0x24;
  `event_image_open` (0x26F930) closes it with 0x25 and frees it when that
  ends; `event_image_close` (0x26F9B0) moves the chip window into the
  character window with anim 0x1F while object 9 fades −0x20 a frame. The
  portrait op drives the same window: 0x4EF5E0 closes it at count 0 as a
  different character comes in; 0x4EF550 at count 8 focuses the new
  character and opens its chip's window. Both run before the count moves
  on (0x26F600), so a first portrait (count from 8) opens it at once.
- **Ambient loops** (0x271070): VM+0x6D8 = n; the playing SE stops through
  0x26A3B0(handle, 30); n ≠ 0 starts SE (11, 12, 26)[n−1] through
  0x1FB250(se, 30). The 30 is read as a fade in frames (**Medium**). Poll
  0x271110: objects 16–18 by the mask (0, 1, 3, 4)[n] at 0x4390F8; on: a
  hidden object starts (anim 0x23, the third 0x22; the second at speed 1.5)
  from alpha 0 and fades +0x10 a frame; off: −0x10, hidden below 0.

- **Text rate.** 0x20A920 sets the rate; the step interval is 60/rate.
  Typing advances speed/interval glyphs a frame, and a window's default speed
  is the interval. The game loop is not slowed. `glitch` (0x271300/0x271550):
  arg 0 hides the town's 25×20 cells from the counter's old to new value
  (+25 every 16 frames below 100, every 8 below 200, then +15/+20/+25 a
  frame; rate 3 once past 100; done at 500); 0xD/0xC/0xB/0xA show cells
  0–124/125–249/250–374/375–499 (rate 2/2/3/unchanged); 4 shows all (rate
  2); 3 normal rate; 2 rate 20; 1 rate 10. MAIN01T ends at rate 2, so every
  later scene types at rate 2 (**Medium**: inferred from MAIN01T's last
  glitch op; nothing resets it).
- **H.A.V.E.N.** haven_open (0x26B170) plays objects +0x64 anim 6, +0x74 anim
  8 and +0x6C anim 0x20 at (256,258), scale (0.88,1); when +0x64 ends, +0x68
  anim 0x13 and +0x70 anim 0x14. haven_open_2: twelve petals at angle
  i·30+7°, radius 169, alpha clamp((c−i)·32). haven_open_3: rings of 6/8/6
  sprites (24–29, 30–37, 38–43) at i·60+15°/i·45+50°/i·60+30°, radii
  245/256/259. Sprites sit at x = 256 + 0.88·r·sin a, y = 258 − r·cos a,
  rotated by a. rings_out/rings_out_2 fade them out (ring 3, then 1 at −5,
  then 2 at −10).
- **Voice meter** (0x4ECBF0). Seven TAL01 bars (320,9,8,46); the sets at
  0x5977D0 are {f32 low, high, phase (rad), i32 period}. Set 0 idles; set
  VM[0x1AD] runs while a say is pending in window 0 with style 0. Target =
  low + (high−low)·(0.5+0.5·sin(phase + (frame % period)·360/period°)); the
  level moves 0.1 a frame toward it. Bars at x = 26+8i, y = 56+46·(0.5−0.5·level)
  (**Low**: 0x4DE990 may place them relative to the panel).
- **Knocker** (0x2701A0..0x2706F0, 0x4EA6C0). A new object at (x+256, 224−y)
  per file `de%04x`, texture DE0000 or DE0001 by bit f of 0x1807FF.
  knocker_in 0: file 0x13 at (0xA4,−45) in talk scenes, (0xA1,−108) in town
  (−68 when the HUD is in state 2); 1: file 0xF; 2: file 8 runs in from
  x −300 by 40 a frame, hopping over the last 208 px, then anims 1 and 0.
  knocker_out 1 plays file 0xF anim 3, 0 file 0x14. The talk animation
  (0x4ED590) plays file 5/0xF/6 by state while a say types in style 2.
- **Portraits** (0x26F510, 0x4EF1C0, 0x26F600). `talk/<folder>/<folder><hex
  expr>.json`, picture `<folder><hex(expr/5)>` (character 0x1F: expr>>2), at
  (125,189). The same character cross-fades by 8 a frame; another fades out
  over 8 frames, shows its chip icon (TAL01 anim 0x24) and fades in to frame
  21. Mouths (characters 1..0x1C) play anim 1 while talking; eyes
  (1..0x17) blink every 4–6 s (0x4ED6F0). Character 0 removes the portrait
  (**Medium**).
- **Windows and text.** Window and prompt colours at 0x337660/0x337670,
  style text colours per style. A glyph advances
  (((w·256+9)/18·width) >> 4, +8 & ~0xF)/16 and is drawn at x − left·w/18
  in a 16×18 cell scaled by w/18, h/18. A press skips typing (unless
  allow_skip 0); the blip SE 0x0A sounds every 3 frames.

Scripts present dialogue only. Story progress comes from the event table's
completion flags and from town code; no opcode sets a story flag or starts
a battle (**Medium**).

## Guarded addresses

- **Guarded by the decoder:** the code words at 0x2572C0 (city-day row
  geometry), 0x257350 (`% 100`, base 0x42D0F0) and 0x26A920 (event bound
  0x9A); the EVT opcode argc column; regulation ids equal to their row; and
  a valid regulation and day/night value in every match row.
- **Unguarded:** the rumble manager (0x222070 constructor, 0x2225A0
  opponents, 0x222A60 eligibility, 0x222F70 completion, 0x223ED0 status,
  0x2240E0 challenger); entry check 0x259A40; stage advance 0x25A4E0; rota
  lookup 0x257070; schedule screen 0x57A7E0; next day 0x4E8CF0.

## What a native S.L.A.I. Rumble mode needs

**A. Battles only** (Club Wired rota, no story; recommended first). This is
mostly data plus one menu.

1. Export the regulations, match templates and rota with `campaign.py`.
   **Done:** `tables/slai_rumble.json`, below. The shop lineup ids are
   mapped to tables but not to item names yet.
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
  Every opcode is decoded and played by the port (see the opcode table).
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

## Exported table: `tables/slai_rumble.json`

`slai/rumble_table.py` (`register_rumble`) writes the table into the S.L.A.I.
asset root during registration (the installer and `register-slai-assets`).
It is built from the disc on each install and is never committed. Every value
comes from the `campaign.py` decoders, so their guards and checks apply.
`table_match()` reads a match id from this file. It gives the same result as
`scheduled_match()` for every day, city, stage, slot and finals count, and the
tests check this.

- `version`, `schedule_days` (100), `stages` (5).
- `cities[7]`: `{id, name}` in column order 0 Tokyo, 1 Stuttgart, 2 New
  York, 3 London, 4 Hong Kong, 5 Cairo, 6 Las Vegas. `display_order` is
  `[2, 6, 0, 4, 5, 1, 3]` (0x43ACF0).
- `regulations[32]`: `id`, `name` (UK_RUMBLETYPENAME.MSG), and
  `entry_requirement`. `maker` is null for any maker, otherwise 0–4, with
  `maker_name` from Carro, Proton, Hartman, KHT and Zwerg. Also
  `leg_classes` (names), `special` (null or the rule's name), `param_20`
  (raw u16; 65535 on the Dummy rows) and `prize`.
- `matches[157]`: `id`, `regulation`, `night`, `robots_min` and
  `robots_max` (no row exceeds 6 on the disc), `level`, `save_record` and
  `save_record_b`. `weights[10]` holds the raw float32 weights, each written
  as the shortest decimal that reads back to the same float32.
- `rota.days[100]`: `day`, `shop_lineups[5]` (the +0 header) and
  `city_days[7]`, one city-day row id per city column (`column*100 + day`
  on the disc).
- `rota.city_days[700]`: `id`, `stages[5]` of `[day, night]` match ids (−1
  is none), and `shop_lineups[10]`, the u16 ids at +0x28..+0x3A in order,
  with 0xFFFF written as −1. These are +0x28 chips, +0x2A..+0x36 the 7
  module-shop categories, +0x38 module area and +0x3A the 10-day-cycle
  index, at the confidence given in the rota section.
- `finals`: `stage` (3), `fallback[7]` (table 0x334A80, `[day, night]` per
  city) and `rounds[3]`, indexed by finals cleared:
  - each round has `name`, `host`, `host_name` and `matches` `[day, night]`
    (the host's injected ids);
  - with `host_ignores_rota` false, a host slot changes only when it already
    holds a match; the final (New York, `[4, -1]`) ignores the rota;
  - in other cities, a scheduled id in `replace[].from` becomes
    `replace[].with[slot]`; a null `with` means the city's `fallback` pair.
  - With 3 cleared, the rota stands as it is.
- `provenance`: the table addresses, the guarded code words (0x2572C0 and
  0x257350, as hex words), the checks applied, and the logic addresses the
  port mirrors (0x257070, 0x25A4E0, 0x259A40, 0x222A60, 0x223ED0).

### Las Vegas night stage

The disc has no `NIGHT_LASVEGAS.BG`. It ships 7 `DAY_*.BG` scenes and only 6
`NIGHT_*.BG` scenes, and battledvd.bin names only `z_zbattle/bg/day_lasvegas`.
The original therefore fights Las Vegas night matches on the day scene, with
the day package's `VLIGHT.DAT` lighting (**Medium**: the stage loader itself
is not traced).

`LASVEGAS_NIGHT.MED` is not night lighting (traced 2026-10-26, **High**):

- The names at 0x59D480..0x59D570 (`sinjyuku_day`, `sinjyuku_night`, …,
  `lasvegas_day`, `lasvegas_night`, `practice_day`, `practice_night`) follow
  the path `host0:data/z_zbattle/effect/`.
- 0x50A040 takes the save's byte `+0x476A` (city × 2 + night) as an index
  into the pointer table 0x59D580. It appends the name to that path and
  reads a count followed by records, allocating 96 bytes each. A `.MED` file
  is a stage's effect list.
- Both `LASVEGAS_DAY.MED` and `LASVEGAS_NIGHT.MED` are empty (count 0).

So the port's `slai_day_lasvegas` for night matches is the original's
choice. The other cities' effect lists are not imported: the stage importer
looks for `<CITY>.MED`, but the disc names them `<CITY>_DAY.MED` and
`<CITY>_NIGHT.MED`, and it would file them as lighting. Importing them
means decoding the effect records. For example, `SINJYUKU_NIGHT.MED` holds
17 records and `EGYPT_NIGHT.MED` holds 10.

## Shop tables (imported 2026-10-27)

`slai/shop_table.py` reads these and writes `tables/slai_shops.json`; the
port's use is in `docs/SLAI_INTEGRATION.md`.

- **City-day rows.** The rota's +0x28 is the CHIP lineup, +0x2A..+0x36
  the seven module-shop categories (body, legs, arm, shoulder, option,
  paint, emblem; options are not the port's modules), +0x38 the used SV
  row and +0x3A the junk row. 67 CHIP lineups.
- **Dealers.** The day row's +0 header holds a lineup per shop id; 3 is
  Omsk and 4 S&V Ma.Fabrik (0x547950). Their profiles are MSG text.
- **Junk shop (spot 0x0F).** Its row is day `day/10*10 + offset[city] - 1`
  of a ten-day cycle (offsets 0x43A908, 0x51c5a0). Its SV rows 10, 11 and
  12 show rows 0, 4 and 7 until save flag +0xA18 is set (0x51c620); 0x25A7F0
  sets it when the stage ladder reaches 4.
- **CHIPs.** CPU records give S.L.A.I.'s CHIPs; the first is the E-Chip
  (regulation special 7). Faces are `2d/s_chip/<family>.png`.
- **Prices.** Part levels are signed (-99..99) and the price curves'
  `low_scale` is 1, so a part tuned either way costs more than its base.
  S.L.A.I.'s own curves climb far less steeply than Phantom Crash's: at
  level 99 a body costs 45 times its level-0 price, against 241 times.
- **Paints and stickers.** Prices are in the table at 0x41E090 (not
  imported).

## Open questions (most need runtime checks in PCSX2)

- What sets 0x43B91C? (The lineup ids are resolved: see Shop tables.)
- Is `+0xA14` money? What do the regulation's `+0x20` parameter and the 10
  match weights do?
- What builds sit behind the roster ids (f_256CC0)? This needs ranker and
  robot data decoding.
- What are the semantics of save+0xA20 values 7 and 8, of EVT opcodes 33,
  47 and 48, of story trigger kinds 2–4, and of save+0x4448 (the
  tutorial's second ending)?
