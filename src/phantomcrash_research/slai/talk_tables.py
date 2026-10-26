"""S.L.A.I.'s talk-scene tables, read from the PAL executable.

The talk VM (shipped source names talk.cpp, evtcmd.cpp, knocker.cpp) runs the
EVT scripts with an opcode table, a character table and the story event table.
This module turns them into the editable ``tables/slai_talk.json`` the port
loads, and the EVT scripts into JSON. Names are researcher-assigned labels
unless noted; see docs/SLAI_CAMPAIGN_RESEARCH.md, "Talk scenes".
"""

from __future__ import annotations

import struct

from .campaign import (EVT_OPCODE_ARGC, EVT_OPCODE_NAMES, EVT_OPCODE_STRIDE,
                       EVT_OPCODE_TABLE, EVT_STRING_ARGS, evt_opcode_argc, read_evt,
                       story_events)

TALK_OVERLAY = "towndvd.bin"
# The package directory: {name, u16, u16 file count, u32 sector, u32 sectors}.
DIRECTORY_TABLE, DIRECTORY_ROWS, DIRECTORY_STRIDE, SECTOR = 0x34E1D0, 8209, 16, 0x800
# Characters 2..0x39: {folder, display name, u8 chip icon (the icon
# character shown as the portrait changes, 0x26F7B0), u8 ANB count, u8 PIG
# count, pad}; 0x3A on: {icon name, display name}.
CHARACTER_TABLE, CHARACTER_STRIDE, CHARACTER_FIRST, CHARACTER_LIMIT = 0x5C4398, 12, 2, 0x3A
ICON_TABLE, ICON_STRIDE, ICON_LIMIT = 0x5C4650, 8, 0x44
KNOCKER, PARTNER = 0, 1  # character ids 0 and 1 are special-cased by 0x4EECB0
AMBIENT_TABLE = 0x4390F0  # u16 SE ids for opcode 0x2A, index 0 = silence
AMBIENT_COUNT = 4
# ambient_loop's sprite masks by loop (0x271110): bit n shows ambient
# object n (VM+0xA4..0xAC).
AMBIENT_OBJECTS = 0x4390F8
# badge's four joined-character places (0x337A30, s32 x/y pairs in the
# centred frame: x right of the centre, y up).
BADGE_SLOTS, BADGE_SLOT_COUNT = 0x337A30, 4
# badge's name for a character not yet met (flag set): a NUL-ended string
# 0x26EED0 copies to its stack.
BADGE_UNKNOWN_NAME = 0x4390E8
# Opcode 0x10's pictures: u8 numbers n -> talk/news/<n // 100>/news<n>.png;
# ids 0 and 1 show none (0x26D390).
NEWS_TABLE, NEWS_COUNT, NEWS_FIRST = 0x3377FE, 40, 2
# u32 GS RGBA by window style (0x26FC60): window art, then the prompt mark.
WINDOW_COLOURS, PROMPT_COLOURS, WINDOW_STYLES = 0x337660, 0x337670, 4
# Text and nameplate colours by window style, loaded as constants by 0x26FC60;
# style 2 is the Knocker's, 0 the portrait panel's.
STYLE_TEXT = ((0x807D8070, 0x80805E22), (0x805F804B, 0x80197733),
              (0x80807150, 0x8080374B), (0x805F804B, 0x80197733))
# panel_sprites' 64x64 icon places (0x337C20, s16 x/y pairs).
PANEL_SPRITES, PANEL_SPRITE_COUNT = 0x337C20, 7
# login_screen's progress curve (0x3376A0): s16 (frame, percent) to -1.
LOGIN_PROGRESS = 0x3376A0
# login_screen's strings (0x336F58 -> 0x42DD40, 0x42DDB0, 0x42DE20); the PAL
# executable holds English only.
LOGIN_TEXT = {"message": "Please wait while the system is rebooted....{01a9}FIRA{01aa}",
              "label": "% Complete"}
# The panel's voice meter (0x4ECBF0): 7 bars of {f32 low, f32 high, f32 phase
# (radians), i32 period (frames)}; the table pointers at 0x5977D0 are picked
# by VM[0x1AD] while a portrait speaks, the first one otherwise.
VOICE_METER_TABLES, VOICE_METER_SETS, VOICE_METER_BARS = 0x5977D0, 3, 7
# Knocker files whose bit is set use DE0000.PIG, the rest DE0001.PIG (0x4EA6C0).
KNOCKER_TEXTURE_MASK, KNOCKER_FILES = 0x1807FF, 0x15
TALK_LANGUAGES = {"uk": "TALK/EVENT/UK", "us": "TALK/EVENT/US", "de": "TALK/EVENT/DE",
                  "fr": "TALK/EVENT/FR"}
# Text control colours: the GS RGBA constants the text renderer (0x207400)
# loads for 0xFF91..0xFF9B and the name/value tokens; alpha 0x80 = opaque.
STYLE_COLOURS = (0x8080374B, 0x802F2F7C, 0x80267878, 0x80267878, 0x80197733, 0x80002247,
                 0x80404018, 0x80404040, 0x80774477, 0x80004B77, 0x80805E22)
TOKEN_COLOURS = {"highlight_box": 0x80775030, "highlight_glyph": 0x80010101,
                 "value": 0x807D8070, "save_name": 0x80197733, "player": 0x80805E22}

# The scene ids 0x262980 switches between (constructor table 0x3358B0 and
# the debug print "New Main Mode=%d"); names follow the nearby .cpp strings.
SCENES = {
    0: "title", 4: "townmap", 5: "nameentry", 6: "townmap", 7: "garage", 8: "assemble",
    9: "transport", 0x0A: "trans3d", 0x0B: "moduleshop", 0x0C: "plustech", 0x0D: "cdedit",
    0x0E: "chipshop", 0x0F: "shopimp", 0x10: "garage", 0x11: "ending", 0x13: "personal",
    0x14: "schedule", 0x15: "nextday", 0x16: "systemmenu", 0x17: "svspec", 0x18: "assemble",
    0x1B: "town_avatar", 0x1C: "town_avatar", 0x1D: "battle_return", 0x1E: "menubase",
    0x1F: "tutorial", 0x20: "menubase", 0x21: "ending", 0x22: "ending", 0x23: "ending",
    0x25: "vsmode", 0x26: "talk", 0x27: "arcade", 0x29: "menubase", 0x2A: "wired",
    0x2B: "talk", 0x2C: "talk", 0x2E: "mrdupnp", 0x2F: "mrdupnp", 0x30: "talk",
    0x31: "loginmenu",
}
ANY_PLACE = 0x32  # place / sub-place values from here on match anything
# Trigger kinds: 0 on a scene change (place = from, sub = to), 1 on entering
# the town map, 2 the town map's second check (0x26A8A0); 3 and 4 unplaced.
EVENT_KINDS = {0: "scene_change", 1: "town", 2: "town_late", 3: "kind_3", 4: "kind_4"}
EVENT_FLAGS = {1: "talk_background", 2: "skip_load", 8: "reset", 0x40: "freeze_logo",
               0x80: "town_overlay"}
# Condition functions (0x26A990..0x26AF40) by what they test; unnamed ones
# keep their address.
CONDITIONS = {
    0x26A990: "tutorial_cleared_without_4448", 0x26A9D0: "tutorial_cleared_with_4448",
    0x26AA10: "tutorial_not_cleared", 0x26AA40: "never",
    0x26AB90: "money_over_999", 0x26ABE0: "money_over_572", 0x26AC30: "money_over_99",
    0x26AC80: "completion_score", 0x26AD40: "sponsor_offer_open",
    0x26ADB0: "sponsor_offer_due", 0x26AE10: "sponsor_current",
    0x26AE60: "sponsor_level_1", 0x26AED0: "sponsor_level_2", 0x26AF40: "sponsor_level_3",
}
# Progress thresholds (0x26A570): won ranker records (0x30..0x67), then
# regional (0x68..0x6E) from 0x39, then finals (0x1A..0x1C) from 0x40.
THRESHOLD_REGIONAL, THRESHOLD_FINALS = 0x39, 0x40
TUTORIAL_RECORD = 0x18  # 0x257410: save+0x168 == 1, record 0x18's cleared byte


def _u32s(program, va, count):
    return struct.unpack("<%dI" % count, program.read(va, 4 * count, TALK_OVERLAY))


def _string(program, va):
    return program.read(va, 64, TALK_OVERLAY).split(b"\0")[0].decode("latin1") if va else ""


def package_directory(program, wanted=("TALK", "FONT", "2D/S_CHARA", "2D/S_CHIP")):
    """Shipped folder names -> package offsets, for the folders asked for."""
    table = program.read(DIRECTORY_TABLE, DIRECTORY_ROWS * DIRECTORY_STRIDE, TALK_OVERLAY)
    folders = {}
    for row in range(DIRECTORY_ROWS):
        name_va, _, count, sector, _ = struct.unpack_from("<I2H2I", table, row * DIRECTORY_STRIDE)
        name = _string(program, name_va)
        if name.startswith(wanted):
            folders[name] = {"offset": sector * SECTOR, "files": count}
    return folders


def characters(program):
    """Talk characters: portraits for 2..0x39, icon-only entries from 0x3A."""
    out = {}
    for char in range(CHARACTER_FIRST, CHARACTER_LIMIT):
        row = program.read(CHARACTER_TABLE + char * CHARACTER_STRIDE, CHARACTER_STRIDE,
                           TALK_OVERLAY)
        folder, name = struct.unpack_from("<2I", row)
        out[char] = {"folder": _string(program, folder), "name": _string(program, name),
                     "chip_icon": row[8], "expressions": row[9], "pictures": row[10]}
    for char in range(CHARACTER_LIMIT, ICON_LIMIT):
        icon, name = struct.unpack("<2I", program.read(
            ICON_TABLE + (char - CHARACTER_LIMIT) * ICON_STRIDE, ICON_STRIDE, TALK_OVERLAY))
        out[char] = {"icon": _string(program, icon).lower(), "name": _string(program, name)}
    return out


def opcodes(program):
    """The 74 opcodes: name, argument count, wait mask and whether they poll."""
    argc = evt_opcode_argc(program)
    table = program.read(EVT_OPCODE_TABLE, EVT_OPCODE_STRIDE * len(argc))
    out = []
    for op in range(len(argc)):
        _, poll, _, _, mask = struct.unpack_from("<4IQ", table, op * EVT_OPCODE_STRIDE)
        waits = [EVT_OPCODE_NAMES[bit] for bit in range(len(argc)) if mask >> bit & 1]
        out.append({"op": op, "name": EVT_OPCODE_NAMES[op], "argc": argc[op],
                    "polls": bool(poll), "waits_for": waits,
                    "string_arg": EVT_STRING_ARGS.get(op, -1)})
    return out


def _scene(place):
    return SCENES.get(place, "any" if place >= ANY_PLACE else "scene_%02x" % place)


def talk_events(program):
    """The story event table with its kinds, scenes and conditions named."""
    out = []
    for row in story_events(program):
        if not row["name"]:
            continue
        flags = [name for bit, name in EVENT_FLAGS.items() if row["flags"] & bit]
        condition = row["condition_fn"]
        out.append({
            "index": row["index"], "name": row["name"],
            "kind": EVENT_KINDS.get(row["trigger"], "kind_%d" % row["trigger"]),
            "flags": flags, "place": _scene(row["place"]),
            "sub_place": _scene(row["sub_place"]),
            "city": row["save_a20"] if row["save_a20"] < 9 else -1,
            "threshold": row["progress_threshold"],
            "condition": CONDITIONS.get(condition, "fn_%06x" % condition) if condition else "",
            "condition_arg": row["condition_arg"],
            "required_flag": row["required_flag"], "blocking_flag": row["blocking_flag"],
            "completion_flag": row["completion_flag"],
        })
    return out


def _voice_meter(program):
    sets = []
    for pointer in _u32s(program, VOICE_METER_TABLES, VOICE_METER_SETS):
        data = program.read(pointer, 16 * VOICE_METER_BARS, TALK_OVERLAY)
        sets.append([{"low": round(low, 4), "high": round(high, 4), "phase": round(phase, 4),
                      "period": period}
                     for low, high, phase, period in struct.iter_unpack("<3fi", data)])
    return sets


def _login_progress(program):
    out = []
    for at in range(LOGIN_PROGRESS, LOGIN_PROGRESS + 64, 4):
        frame, value = struct.unpack("<2h", program.read(at, 4, TALK_OVERLAY))
        if frame < 0:
            return out
        out.append([frame, value])
    raise ValueError("login_screen progress curve has no end")


def talk_table(program):
    """Everything tables/slai_talk.json holds."""
    ambient = struct.unpack("<%dH" % AMBIENT_COUNT,
                            b"\0\0" + program.read(AMBIENT_TABLE, 2 * (AMBIENT_COUNT - 1),
                                                   TALK_OVERLAY))
    window = _u32s(program, WINDOW_COLOURS, WINDOW_STYLES)
    prompt = _u32s(program, PROMPT_COLOURS, WINDOW_STYLES)
    panel = struct.unpack("<%dh" % (2 * PANEL_SPRITE_COUNT),
                          program.read(PANEL_SPRITES, 4 * PANEL_SPRITE_COUNT, TALK_OVERLAY))
    news = program.read(NEWS_TABLE, NEWS_COUNT, TALK_OVERLAY)
    badges = struct.unpack("<%di" % (2 * BADGE_SLOT_COUNT),
                           program.read(BADGE_SLOTS, 8 * BADGE_SLOT_COUNT, TALK_OVERLAY))
    return {
        "opcodes": opcodes(program),
        "characters": {str(k): v for k, v in characters(program).items()},
        "events": talk_events(program),
        "scenes": {str(k): v for k, v in SCENES.items()},
        "ambient_sounds": list(ambient),
        "ambient_objects": list(program.read(AMBIENT_OBJECTS, AMBIENT_COUNT, TALK_OVERLAY)),
        "badge_slots": [list(badges[i:i + 2]) for i in range(0, len(badges), 2)],
        "badge_unknown_name": program.read(BADGE_UNKNOWN_NAME, 4, TALK_OVERLAY).split(b"\0")[0]
                                     .decode("ascii"),
        "news_pictures": ["talk/news/%d/news%03d.png" % (n // 100, n) if i >= NEWS_FIRST else ""
                          for i, n in enumerate(news)],
        "style_colours": ["%08x" % c for c in STYLE_COLOURS],
        "token_colours": {k: "%08x" % v for k, v in TOKEN_COLOURS.items()},
        "window_colours": ["%08x" % c for c in window],
        "prompt_colours": ["%08x" % c for c in prompt],
        "style_text_colours": [["%08x" % c for c in pair] for pair in STYLE_TEXT],
        "panel_sprites": [list(panel[i:i + 2]) for i in range(0, len(panel), 2)],
        "login_progress": _login_progress(program),
        "voice_meter": _voice_meter(program),
        "login_text": dict(LOGIN_TEXT),
        "knocker_textures": ["talk/de000/de0000.png" if KNOCKER_TEXTURE_MASK >> f & 1
                             else "talk/de000/de0001.png" for f in range(KNOCKER_FILES)],
        "tutorial_record": TUTORIAL_RECORD,
    }


def _signed(value):
    return value - 0x10000 if value >= 0x8000 else value


def evt_json(data):
    """An EVT script as JSON: instructions [name, args...] and its strings."""
    script = read_evt(data)
    code = [[EVT_OPCODE_NAMES[op], *(_signed(a) for a in args)]
            for _, op, args in script["code"]]
    return {"code": code, "strings": script["strings"]}


def json_evt(script):
    """Inverse of evt_json, for edited scripts."""
    from .campaign import text_glyphs
    names = {name: op for op, name in EVT_OPCODE_NAMES.items()}
    code = bytearray()
    for name, *args in script["code"]:
        op = names[name]
        if len(args) != EVT_OPCODE_ARGC[op]:
            raise ValueError("%s takes %d arguments" % (name, EVT_OPCODE_ARGC[op]))
        code += struct.pack("<%dH" % (len(args) + 1), op, *(a & 0xFFFF for a in args))
    strings = [struct.pack("<%dH" % (len(units) + 1), *units, 0xFFFF)
               for units in (text_glyphs(s) for s in script["strings"])]
    count = len(strings)
    at = 4 + 4 * (count + 1)
    offsets = [at]
    body = bytes(code) + b"\0\0" * (len(code) // 2 % 2)
    for blob in strings:
        offsets.append(at + len(body))
        body += blob
    return struct.pack("<%dI" % (count + 2), count, *offsets) + body
