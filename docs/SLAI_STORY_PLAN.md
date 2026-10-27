# S.L.A.I. story: talk scenes in the port (plan, 2026-10-21)

Goal: play S.L.A.I.'s story as its own talk scenes, faithful in look and
timing, inside Phantom Crash's town and Club Wired, triggered by the shared
save. This sits beside S.L.A.I. Rumble (docs/SLAI_RUMBLE_PLAN.md): Rumble
supplies the battles and records, the story reads them. Neither changes the
other's screens.

Research and provenance: docs/SLAI_CAMPAIGN_RESEARCH.md ("Story event
table", "Talk scene assets", "EVT script format and interpreter").

## Status

- **Done.** Extraction (`register-slai-talk`, also part of the S.L.A.I.
  install); the scene player for every opcode; the event selector on the
  shared save; the screen hook. The opening plays: MAIN01T, MAIN02K and
  MAIN03T in order on the first town-map visit, the Battle School scenes
  BS01K/BS03K/BS04K/BS05K, and MAIN05K (pro license). The later scenes'
  features play too: joined characters' badges, the chip and character
  windows, the partner chip and its paths, and the ambient loops. All 146
  event scenes run to their end. Every place but city travel has a port
  screen, Battle School holds the tutorial battle, and the counts the
  conditions read (nights, enemies shot down, sponsors, completion score)
  are kept on the shared save.
- **Not yet.** The junk module shop (MAIN32K), city travel
  (transport/trans3d), the MOD and decal collections, and the battle score
  the original computes (an estimate stands in; see "Counts and sponsors").

## Where scenes play

The event table names S.L.A.I.'s places. `data/story/slai_talk.json`
`places` maps them to the port's screens; a scene plays on a move between
screens whose places match a row (kind `town`: on entering the town map;
kind `scene_change`: from `place` to `sub_place`).

| S.L.A.I. place | rows | port screen |
|---|---|---|
| townmap (town rows and moves from it) | 60 town + 29 | `town_map` |
| wired | 26 + 4 | `club_wired`, `slai_rumble` (Rumble S.L.A.I.) |
| garage | 1 | `garage` |
| moduleshop | 1 | `the_mechanist` (only MAIN32K, at the junk shop: none yet) |
| cdedit | 1 | `blood_music` |
| ending | 1 | `slai_battle` → `slai_result`: the battle that wins the final (MAIN72E needs all three finals, in New York) |
| tutorial (Battle School) | 1 | `slai_battle_school` |
| battle_return | 42 + 4 | `slai_battle` (the move into a battle, and out of it) |
| nextday | 42 + 4 | `slai_result` (a S.L.A.I. battle's end) |
| transport/trans3d | 3 | none: city travel is not traced; the rows repeat any → nextday rows |
| any | 6 | any screen |

A S.L.A.I. battle leaves by `slai_result`: the port settles the match
(`finish_slai_match`), the story checks the move `slai_battle` →
`slai_result` (S.L.A.I.'s battle_return → nextday), and the player goes
back to the hub the battle was entered from (Rumble S.L.A.I. or Battle
School), after any scene. The move into a battle from the hub is wired →
battle_return (MAIN11K, MAIN16K, MAIN59K, MAIN86K). Phantom Crash's own
battles have no S.L.A.I. place, so BS05K ("not cleared") does not follow
them.

**Battle School** (`ui/slai_battle_school.json`, from the town map's
Battle School) is Rumble S.L.A.I.'s hub listing only the tutorial matches
(records 0x18 and 0x19), once each however many cities hold them that day;
Rumble S.L.A.I. leaves them out. The rota offers them at stage 0, so the
school is open on the days the original's is. EXIT goes back to the town
map.

A scene plays where the original plays it: town scenes (MAIN01T, MAIN03T)
over the screen they interrupt, which keeps running underneath; talk scenes
(the `talk_background` rows) on their own H.A.V.E.N. background, cut to the
4:3 picture. After a scene the move it interrupted is checked again, so
chained rows (MAIN01T → MAIN02K → MAIN03T) play one after another, as in
S.L.A.I.

## Triggers on the shared save

`QuestSave::slai_story` (`slai_story_save.hpp`, saved as `slai_story`):

- `flags`: the 256 event flags (`save+0x426F`): the rows' required,
  blocking and completion flags. A scene sets its completion flag as it
  starts, so it plays once. When a move plays nothing, the rows flagged
  `reset` (MAIN86K) may play again (0x26A7D0).
- `days` (`save+0x41DC`): nights slept since the story began, up to 9999.
  The port counts them from `QuestSave::total_days` (`nights_seen` notes
  where it last caught up; the first look only notes it), and each night
  sets the overnight flag pairs (0x4E8D70: 27→28, 37→38, ... 123→124).
- `defeated` (`save+0x41CC`): enemies shot down in S.L.A.I. battles.
- `sponsor`, `previous_sponsor`, `sponsors_had`, `shot_down_streak`,
  `dismissal`, `survived`: see "Counts and sponsors".

| condition | source |
|---|---|
| city (`save+0xA20`) | `slai_rumble.city` |
| progress threshold | cleared ranker records 0x30..0x67, regional 0x68..0x6E and `slai_rumble.finals_cleared`, as one count |
| tutorial cleared, first day or later | `slai_rumble.records[0x18].cleared` and `days` |
| defeated over 99, 572, 999, 4999 | `defeated`; a milestone passed over marks its row done |
| last battle lost or survived | `survived` |
| sponsor offer, current, level 1..3 | the sponsor fields |
| completion score | over 999 of 1000 (0x24B5A0) |
| all mods, decals, music | the collection counts: music counts the S.L.A.I. songs owned (credits 81..170); mods and decals are 0 (no port equivalent) |
| junk module shop | the screen entered is listed in `junk_shops` (none yet) |

With S.L.A.I. off nothing reads or writes the block. Base game and S.L.A.I.
stories both run when S.L.A.I. is on; the base game's events play first on
a move they share.

## Counts and sponsors

After each S.L.A.I. battle (`slai_story_after_battle`, from global +0x489C):
`defeated` grows by the enemies shot down, `survived` records the result
and the sponsor before the battle becomes `previous_sponsor`. Then, unless
the battle is excluded (+0x489E, not traced; the port excludes none):

- **Losing a sponsor (0x24A9E0).** Outside the head-to-head matches, a
  score under 10 dismisses it (reason 3) and being shot down twice in a row
  does (reason 2); surviving resets the run. In any match, a body whose
  brand is not the sponsor's dismisses it (reason 1). Brands are the body
  table's +2 (`SlaiRobot::maker`): Hartman keeps 2, Proton 1 and 5, Carro
  0 and 7, KHT 3 and 6, Zwerg 4. A Phantom Crash body has none.
- **An offer (0x25A4E0).** A score over 14 in a maker's rumble
  (regulations 6..10) or, in the body-type rumbles (11..14, 16..21), by the
  body's brand, brings that maker: it becomes the sponsor and joins
  `sponsors_had`, and the shot-down run starts over.

Sponsors are numbered 0 Hartman, 1 Proton, 2 Carro, 3 KHT, 4 Zwerg (5 none),
not as the regulation makers are. A sponsor condition holds only on the
moves after the change (current differs from previous): an offer is "open"
unless one of the flags 107, 111, 115, 119, 123 is set ("due"); "level n"
is the previous sponsor leaving for reason n. All the rules are data in
`tables/slai_talk.json` (`sponsor_rules`, `defeat_milestones`,
`completion_score`, `overnight_flags`).

**The score.** The original's battle score (the player's battle struct
+0x24) is not traced. The port uses shootdowns × `score_per_shootdown`
(10, `data/world/slai_rumble.json` balance), an estimate that makes the
sponsor thresholds (10 to keep, 15 for an offer) reachable as in play.

**The completion score** (0x24B5A0) adds, each capped and scaled to its
points: ranker records 0x30.. (56, 3920), regional 0x68.. (7, 700), finals
0x1A.. (3, 600), records 0x20.. (16, 800), collections (238, 2500), decals
(229, 500), music (90, 480) and sponsors had (5, 500); the sum over 10,
rounded half up, is 0..1000. REC_DATA05K needs more than 999, which the
missing collections keep out of reach for now.

## Name placeholders

Scripts use three tokens (MSG 0xFFFA/0xFFFB/0xFFFC):

- `{player}`: the shared save's player name (braces dropped).
- `{partner}`: the partner chip's name: the CHIP fitted in the hangar
  ridden, by its NAME ENTRY name (else its record's), as S.L.A.I. reads it
  from the current hangar.
- `{alias}`: the player's wire-head alias, "ALIAS" until the port has a
  place to enter it (S.L.A.I.'s name entry asks for both names).

## The partner chip

S.L.A.I. picks a script's case by the ridden chip's CPU type (VM+0x770:
types 2..9 play cases 1..8; 0 the Knocker's). Phantom Crash chips have no
CPU type, so `data/story/slai_talk.json` `partner.selectors` gives each
chatter personality (0..4) a case, a port choice, and `partner.pictures`
gives each chip family its S.L.A.I. chip picture (Dog I → `dog1`, ...). A
chip without a picture (prototype and enemy chips), or no chip, leaves the
Knocker.

With a partner, the partner ops (0x270790, 0x2708D0) play TAL01's chip frame
(object 15, anims 0x29 and 0x2A) with the chip's picture where TAL01 has
the icon; leaving hides the nameplate, and `knocker_only_pose` does nothing.
`partner_path` (0x271980, polled by 0x271A40) moves the frame a step a
frame along one of the three paths at 0x42DED0 (up, or right when the path
is horizontal), the nameplate's glow (anim 0x2B) with it; a count of 0
repeats until `partner_path_stop`, and path 2 counts frames (7 a pass). In
town S.L.A.I. shows the partner in its HUD's chip window, which the port
does not have; the talk scene's frame stands in.

## European languages

The S.L.A.I. disc carries UK, US, German and French scripts (and Japanese,
not exported). All four are installed under `talk/event/<language>/`; the
font pages hold their accented glyphs. `data/story/slai_talk.json`
`language` picks one (default `uk`); a missing scene falls back through uk,
us, de, fr. A later settings entry can set it; the names in the character
table are the same in every language.

## The scene player

Core (`port/src/slai_*`, no SDL):

- `slai_anb`: ANB banks and objects (step, tween, quads).
- `slai_talk_data`: the talk table, scripts, font.
- `slai_talk_scene` (+ `_stage`, `_effects`, `_text`, `_cast`, `_draw`):
  the VM (0x4EB560): pending ops polled each frame, ops run until one
  waits; branching on the case selector; draws as quads in the PS2's
  512×448 screen.
- `slai_story`, `slai_story_rules`: loading, the event selector, places;
  nights, sponsors and the completion score.
- `slai_talk_partner`: the partner chip's ops.

Desktop: `SlaiTalkScreen` (`desktop_slai_talk.cpp`) draws the quads over the
640×480 canvas with SDL geometry (additive cells additive), plays the
scene's BGM and cues; `desktop_screens_slai_story.cpp` hooks the selector
into screen moves. `--start slai_talk:<scene>` plays one scene.

Buttons: A or B pages and skips typing (S.L.A.I.'s X and circle); both
triggers held hurry the typing; START skips the scene (a port addition).

## Port choices and open points

- **Text rate.** MAIN01T's glitch leaves the rate at 2 for the rest of the
  game; scenes start at 2 (`text_rate`), MAIN01T at 60. MAIN03T starts with
  the town hidden (`glitched`), as MAIN01T left it.
- **Town HUD.** The port's town has no S.L.A.I. HUD; town scenes keep its
  state for the text and Knocker heights and draw a translucent bar behind
  their text (a port addition). Black glitch cells and the bar reach a wide
  window's edges.
- **Text** is drawn from the original font pages; sharp text is a later
  option (memory: sharp UI only for text, lines and simple shapes).
- **Ambient loops** are the rendered cues 11, 12 and 26 (each one pass of
  its Sony loop region), looped by the mixer and faded over 30 frames by
  gain moves.
- Assumptions to check at runtime in PCSX2: the voice meter's placement,
  `end_case` resetting the case, character 0 removing a portrait, the
  threshold as one running count, cues 11, 12 and 26; the badge name's
  colour (0x805E22 written over 0x807D8070 by 0x207510) and the text flag
  0x10 (placed here from its top left); the ambient SE's 30 as a fade-in;
  VM+0x30, which event_portrait and the portrait's count 8 clear (not
  modelled).

## Next steps

1. The junk module shop (town spot 0x0F) as a port screen, then list it in
   `junk_shops` (MAIN32K).
2. The battle score as S.L.A.I. computes it, replacing the estimate.
3. MODs and decals as S.L.A.I. collections, for the completion score.
4. NEWS_T (threshold rows) once Rumble records fill in.
5. A language setting in the S.L.A.I. settings page.
