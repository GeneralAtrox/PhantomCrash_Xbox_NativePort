# NEW GAME (research)

What the original does after NEW GAME on the main menu, and what the port
needs to do the same. Static analysis of the Europe XBE in Ghidra; nothing
here has been watched running yet.

## Headline: the talking characters are 2D

The characters who talk to the player are not real-time 3D models. Each one
is a pre-rendered 3D sprite sheet, `scroll/mht25_yuki_tex` … `mht40_glik_tex`
(two pages each) and `sht01_naomi_tex` … `sht23_board_tex`:

- nine full-body poses;
- a column of cut-out faces: mouth closed, half open, open, and a blink.

The event player animates them in 2D (opcode 4 below). It changes poses,
blinks at random, flaps the mouth while the text types, and fades characters
in and out. The only human-shaped 3D data on the disc is the mechs (Holy,
Photon, Aeron, drones, bosses), which the garage and battles need, not NEW
GAME. So NEW GAME needs no 3D renderer; it reuses the port's sprite layers,
font, tickers, message box and movie player.

## Scene flow

The scene table is at `0x256738`. Each factory sets the scene id
(`0xe07b0`), creates the scene's task and picks the next scene from its
result (`0xa1bf7`).

| Scene | What it is | Next |
|---|---|---|
| 21 main menu | NEW GAME: result 0 (`0xa2268`) | 28 |
| 28 (`0xa2a53`, task `0xb1af1`) | A wrapper with a 0x113cc-byte state. It stops the music and, after the fade-in, runs the scene whose id is at `0x25b48c` as its child (`0xa1ae8`); that id is 9 and nothing changes it. | 40 on quit |
| 9 (`0x570c0`, `0x571a3`) | Save checks: an existing save (`0x6b040`), free space (`0x6b0cf`, `0x6b1ed`) and message boxes (strings at `0x21cc44`); a cancel goes back to the title. No music. | 0 → 46, 1 → 40 |
| 46 (`0x9f250`, `0x9f381`) | Creates the save (0x5fea4-byte state, `0x6b99b`), with a free-blocks check (`0x6c3d6` < 0x1000) and messages (`0x2002cc`, `0x254dd0`). LOAD GAME (17) also comes here. | 0 → 25, 1 → 39 (garage) |
| 25 (`0x2e692`, `0x2e663`) | Plays movie 0, `opening.sfd` (`0xaebf6(0)`), which can be skipped (`0x2e4fd`). Then points name entry at the player's name, save + 0x280c, mode 0 (`0x2c5b4`). | 0 → 22, 1 → 40 |
| 22 (`0x2c614`) | NAME ENTRY (see below). Entered from 25 it returns result 0. | 0 → 8 |
| 8 (`0x1bec1`, `0x1bd93`) | Club Wired's menu (`club_wird_tex`, 4TOWNMAP), with six help lines (`0x1db1c0`): Rumbling, "The shop guide, Pepper, will show you around", records, schedule, change alias (back to name entry), exit. | 3, 19, 29, 32, 22, 41 |
| 19 (`0x299fe`) | Pepper's guide: a question menu ("What is Rumbling?", …) from the table at `0x1de070` (35 records of {short group, short id, char* title, int}). It returns to the scene that opened it. | caller |
| ~50–158 | Story events (below). Each loads EVENTSEG (`0xa3e32`), runs one event, unloads it and goes to scene 159. | 159 |

The game's first conversation is event scene 144 (`0xa83f7`, event
`0xca100`, script at `0xdb1ae8`):

> "Welcome to Shinjuku Wired, where apathy and freedom abound…" · "I am
> Salsa, the store guide, a Cat Chip." · "Pepper! Will you get up now?" ·
> "Oh...hi there. Welcome to the battlefields of Rumbling…"

Still open: what starts scene 144 after name entry. No fixed scene-table
edge leads to it, so it comes from a story dispatcher; scene 159 is next to
look at.

## Name entry (scene 22)

- Textures `name_entry_tex` (the background, the keyboard frame, the tabs
  Alphabet / Symbols / End, and the labels CHIP NAME, YOUR NAME, PROGRAM
  NAME and SV NAME) and `scr_common_tex`.
- A 15 × 6 grid on each of three pages. The character under the cursor is
  `0x1f99e8[page * 90 + row * 15 + column]` (a string, mapped to a glyph by
  `0xbde14`). Names hold up to 8 glyphs, in the buffer at `0x1f94d4`.
- Controls: the cursor helper with flags `0x17`/`0x12`; A types a
  character; L (`0x400`) deletes (sys effect 21) and R (`0x800`) steps
  forward (sys 5); START jumps to End (sys 9). Changing page restarts its
  help line (three lines at `0x1fa324`, a text box at y 37).
- End checks the name (`0x2d723`). An empty or taken name opens a message
  ("Do you wish to end the name entry without giving your Scoobee a name?",
  "A Scoobee of the same name is already…"). The result then depends on the
  calling scene: 8 → 0, 48 → 1, 4 → 2, 34 → 3, 15 → 4, 14 → 5, 33 → 6, and
  anything else → 0.
- Mode 0-3 (`0x2c5d9`) picks which name is edited: player, Chip, Scoobee
  or program.
- The CURRENT MUSIC ticker is at y 419 (`0x2a85a(…, 0x1a3, 0x17)`).

## Event scripts (EVENTSEG)

`EVENTSEG` (`0xd4c820`–`0xec66a8`, 1.5 MB) is loaded on demand. Each event
scene's creator makes a task (`0xd19ea`) whose setup (`0xd1615`) reads its
record in the event table at `0x1d0e00`. There are 110 records of 36 bytes,
keyed by scene id:

- +0x16: the background index into `0x1d0dd0`, the talk_* backgrounds
  (`talk_garage`, `talk_clubwired`, …);
- +0x17: the cast, up to 6 character indices ending with -1;
- +0x1e: the music (0xCA keeps the current track);
- +0x20: a float.

It loads the talk UI (`0x1d0d88`: scr_common, talk_tex and the six
backgrounds) and the cast's sheets into six slots (`0xd158a`, `0xd106a`),
then starts the interpreter on the event's script (`0xd0f07` → `0xc57e0`).

The interpreter (`0xc5a1a`) steps through 52-byte commands; the first u32 is
the opcode, dispatched through `0x291310`. Opcode 2 yields until the next
frame and opcode 13 ends the script.

| Op | Handler | Meaning (so far) |
|---|---|---|
| 0 | `0xc5b0b` | reset state |
| 1 | `0xc5b9d` | message: a speaker record, then text typed a glyph at a time (A or B shows all), optional ruby |
| 2 | `0xc604b` | wait N frames, or for a press when N < 0; clears per-page state |
| 3 | `0xc620c` | draw a sprite |
| 4 | `0xc62ff` | draw a character: {character, pose, x, y, layer, flags 1 fade in / 2 fade out / 4 no mouth} |
| 5 | `0xc6a0f` | wait for a press |
| 6-9 | `0xc6aef`… | small state and flag changes |
| 10 | `0xc6bc4` | input wait with a result |
| 11, 12 | `0xc6c7f`, `0xc6d4e` | clear flags 2 and 1 |
| 13 | `0xc6e1b` | end |

Opcode 4 in detail: `0xc79c0` gives a character's record at
`0x1c42e8 + 40n` (28 characters: a resource id and nine pose indices), and
`0xc7a77(character, pose)` gives the pose's sprite set. That set holds the
body, the mouth closed / half / open, the blink, and each overlay's byte
offsets.

- **Blink:** drawn for about 4 frames at random intervals of up to a
  second.
- **Mouth:** while text types (flag 8 at `0xa69cbc`) the mouth steps
  closed → half → open → half, one frame every 5 ticks.
- **Fades:** they scale the alpha over the fade time.

Text is stored as the system font's glyph indices, u16 each, not Shift-JIS,
which is why string searches miss it. Codes: `0xFFFF` ends a record,
`0xFFFE` breaks the line, `0xFFEF`/`0xFFF0`/`0xFFF5`/`0xFFF6` start colours
and `0xFFF7` resets them, `0xFFF3` is probably a pause, and `▼` waits for a
button. Speaker names ("Salsa", "Pepper") are records of their own.

Still open:

- Event 144's cast byte is 7, which is `mht32_mion_tex`, yet the lines are
  spoken by Salsa and Pepper. Either the cast index is mapped differently or
  opcode 4 names the character directly. It will show up once the
  converter decodes whole scripts.
- Salsa (a Cat Chip) may speak without a sprite.

## What to build

In order, all on the existing 2D stack:

1. **Converter:**
   - Decode EVENTSEG into `tables/events.json`: per event, its record
     (background, cast, music) and its commands with the text as Unicode.
   - Decode the character table and pose sprite sets (`characters.json`).
   - Decode the talk backgrounds and the name-entry grid.

   Every file stays editable, so mods can rewrite a conversation.
2. **Core `EventPlayer`:** the opcodes above, the typewriter, waits, and the
   pose, blink and mouth state, unit-tested against decoded scripts.
3. **Desktop talk screen:** the background, the message window
   (`talk_tex`), the speaker name, the characters on their layers, and the
   CURRENT MUSIC band.
4. **Name entry:** screen, data file and tests, reusing DrawList, the font
   and the message box.
5. **NEW GAME flow:**
   - Save checks as message boxes. The PC version has no memory-unit
     limits, so scenes 9 and 46 reduce to "create the save" plus their
     failure messages.
   - The opening movie through the intro movie player, then name entry,
     then Club Wired, then event 144.
6. **Club Wired menu** and Pepper's guide (scene 19) with the same pieces.

A 3D renderer (SDL3 GPU, the mech models, motions, shaders) is needed later
for the garage (39), the hangar and battles, not for NEW GAME.
