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
  install); the scene player for every opcode the opening uses; the event
  selector on the shared save; the screen hook. The opening plays:
  MAIN01T, MAIN02K and MAIN03T in order on the first town-map visit, the
  Battle School scenes BS01K/BS03K/BS04K/BS05K, and MAIN05K (pro license).
- **Not yet.** The partner robot (partner_in/out/path), badges, event
  images and portraits, ambient loops (none in the opening); the sponsor and
  completion-score conditions; a S.L.A.I. Battle School battle in the port.

## Where scenes play

The event table names S.L.A.I.'s places. `data/story/slai_talk.json`
`places` maps them to the port's screens; a scene plays on a move between
screens whose places match a row (kind `town`: on entering the town map;
kind `scene_change`: from `place` to `sub_place`).

| S.L.A.I. place | rows | port screen |
|---|---|---|
| townmap (town rows and moves from it) | 60 town + 29 | `town_map` |
| wired | 26 + 4 | `club_wired` |
| garage | 1 | `garage` |
| moduleshop | 1 | `wild_machines`, `plustech`, `sonic_amp` |
| ending | 1 | `ending` |
| tutorial (Battle School) | 1 | none yet |
| battle_return → nextday | 42 | none yet |
| transport/trans3d, cdedit | 4 | none (no port screen) |
| any | 6 | any mapped screen |

Battle School and `battle_return` wait for Rumble S.L.A.I.: its S.L.A.I.
matches will be the port's battles that count for the story, and the
tutorial is S.L.A.I. record 0x18. Until then, `--start slai_talk:bs01k`
plays those scenes directly. Mapping `battle_return` to the base game's
`match_result` would play BS05K ("not cleared") after every Phantom Crash
battle, so it stays empty.

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
  starts, so it plays once.
- `tutorial_mark`: `save+0x4448`, which picks BS03K or BS04K.

Everything else comes from the save the game already keeps:

| condition | source |
|---|---|
| city (`save+0xA20`) | `slai_rumble.city` |
| progress threshold | cleared ranker records 0x30..0x67, regional 0x68..0x6E and `slai_rumble.finals_cleared`, as one count |
| tutorial cleared | `slai_rumble.records[0x18].cleared` |
| money (>99, >572, >999) | the shared NY (`QuestSave::money`) |
| sponsors, completion score | not yet in the port: false |

With S.L.A.I. off nothing reads or writes the block. Base game and S.L.A.I.
stories both run when S.L.A.I. is on; the base game's events play first on
a move they share.

## Name placeholders

Scripts use three tokens (MSG 0xFFFA/0xFFFB/0xFFFC):

- `{player}`: the shared save's player name (braces dropped).
- `{partner}`: S.L.A.I.'s partner chip name. The port uses "PARTNER" until
  partner chips exist; then the riding robot's chip name, as S.L.A.I.
  reads it from the current hangar.
- `{alias}`: the player's wire-head alias, "ALIAS" until the port has a
  place to enter it (S.L.A.I.'s name entry asks for both names).

The case selector follows the same partner chip (f_4EA860): 0 now, 1..8
by chip type once chips exist, so branches already pick the right lines.

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
- `slai_story`: loading, the event selector, places.

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
- Assumptions to check at runtime in PCSX2: the voice meter's placement,
  `end_case` resetting the case, character 0 removing a portrait, the
  threshold as one running count, cues 11, 12 and 26.

## Next steps

1. Rumble S.L.A.I.'s tutorial match (record 0x18) and S.L.A.I. battle
   returns: map `tutorial` and `battle_return` to their screens.
2. Partner robot, badges, event images and portraits, ambient loops: the
   remaining opcodes, needed from MAIN06K on.
3. Sponsors and the completion score as save state, then their conditions.
4. NEWS_T (threshold rows) once Rumble records fill in.
5. A language setting in the S.L.A.I. settings page.
