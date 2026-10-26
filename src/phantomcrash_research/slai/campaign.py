"""Read-only decoders for S.L.A.I.'s single-player campaign data.

Covers the MSG text tables, the town EVT scripts, and the executable tables
that schedule and define Club Wired rumbles. All addresses are PAL
SLES-52940 virtual addresses; see docs/SLAI_CAMPAIGN_RESEARCH.md. Field
names are researcher-assigned labels, not original source symbols.
"""

import re
import struct

# --- MSG text --------------------------------------------------------------

GLYPH_ASCII_LIMIT = 0x5F  # glyphs 0x00-0x5E are ASCII 0x20-0x7E
MSG_END = 0xFFFF
MSG_CONTROLS = {
    0xFFFE: "\n",
    0xFFFA: "{player}",
    0xFFFB: "{partner}",
    0xFFFC: "{alias}",
    0xFF9C: "{/style}",
}
MSG_STYLE_FIRST, MSG_STYLE_LAST = 0xFF91, 0xFF9B
_BRACES = (ord("{") - 0x20, ord("}") - 0x20)
_TOKEN = re.compile(r"\{(player|partner|alias|/style|style:\d+|[0-9a-f]{4})\}")


def glyph_text(units):
    """Glyph units -> text; non-ASCII glyphs become {xxxx} tokens."""
    out = []
    for unit in units:
        if unit in MSG_CONTROLS:
            out.append(MSG_CONTROLS[unit])
        elif MSG_STYLE_FIRST <= unit <= MSG_STYLE_LAST:
            out.append("{style:%d}" % (unit - MSG_STYLE_FIRST))
        elif unit < GLYPH_ASCII_LIMIT and unit not in _BRACES:
            out.append(chr(unit + 0x20))
        else:
            out.append("{%04x}" % unit)
    return "".join(out)


def text_glyphs(text):
    """Inverse of glyph_text."""
    named = {v: k for k, v in MSG_CONTROLS.items() if v != "\n"}
    units, at = [], 0
    while at < len(text):
        match = _TOKEN.match(text, at)
        if match:
            token = match.group(0)
            if token in named:
                units.append(named[token])
            elif token.startswith("{style:"):
                units.append(MSG_STYLE_FIRST + int(match.group(1)[6:]))
            else:
                units.append(int(match.group(1), 16))
            at = match.end()
            continue
        char = text[at]
        if char == "\n":
            units.append(0xFFFE)
        elif 0x20 <= ord(char) < 0x20 + GLYPH_ASCII_LIMIT and char not in "{}":
            units.append(ord(char) - 0x20)
        else:
            raise ValueError("Character %r has no MSG glyph" % char)
        at += 1
    return units


def _units_at(data, at, limit):
    units = []
    while True:
        if at + 2 > limit:
            raise ValueError("Unterminated MSG string")
        unit = struct.unpack_from("<H", data, at)[0]
        at += 2
        if unit == MSG_END:
            return units
        units.append(unit)


def read_msg(data):
    """MSG file: u32 count, u32 offsets[count], 0xFFFF-terminated u16 strings."""
    count = struct.unpack_from("<I", data, 0)[0]
    if 4 + 4 * count > len(data):
        raise ValueError("MSG offset table exceeds file")
    offsets = struct.unpack_from("<%dI" % count, data, 4)
    return [glyph_text(_units_at(data, at, len(data))) for at in offsets]


def write_msg(strings):
    encoded = [struct.pack("<%dH" % (len(u) + 1), *u, MSG_END)
               for u in map(text_glyphs, strings)]
    at, offsets = 4 + 4 * len(strings), []
    for blob in encoded:
        offsets.append(at)
        at += len(blob)
    return struct.pack("<%dI" % (len(strings) + 1), len(strings), *offsets) + b"".join(encoded)


# --- EVT scripts -----------------------------------------------------------

EVT_OPCODE_TABLE = 0x336F70  # 74 x {exec, poll, u32 argc, pad, u64 mask}
EVT_OPCODE_STRIDE = 24
EVT_OPCODE_ARGC = (
    0, 1, 0, 0, 0, 0, 0, 0, 0, 0, 0, 4, 1, 0, 0, 4, 1, 1, 0, 1, 1, 2, 1, 3, 3,
    1, 1, 1, 0, 0, 1, 1, 0, 1, 0, 1, 1, 2, 0, 1, 1, 2, 1, 1, 1, 2, 0, 0, 0, 1,
    1, 0, 2, 2, 1, 2, 2, 1, 2, 2, 1, 0, 1, 1, 1, 1, 2, 1, 2, 1, 1, 0, 0, 0,
)
# Researcher labels from the exec routines (0x26AFB0..0x271B90); see
# docs/SLAI_CAMPAIGN_RESEARCH.md, "EVT opcodes".
EVT_OPCODE_NAMES = dict(enumerate((
    "end", "backdrop_in", "haven_open", "haven_open_2", "haven_open_3", "haven_close",
    "rings_out", "rings_out_2", "login_screen", "panel_open", "panel_close",
    "open_window", "close_window", "news_open", "news_close", "preload_backgrounds",
    "background", "event_portrait", "event_portrait_close", "town_hud", "fade_in",
    "fade_out", "load_portrait", "portrait", "badge", "badge_remove", "load_icon",
    "event_image", "event_image_open", "event_image_close", "badge_focus",
    "wait_frames", "wait_button", "say", "load_wait", "knocker_in", "knocker_out",
    "knocker_pose", "load_wait_2", "partner_in", "partner_out", "knocker_only_pose",
    "ambient_loop", "glitch", "panel_sprites", "partner_path", "sync", "sync_portrait",
    "sync_loads", "select_window", "unload_character", "unload_all", "window_style", "case",
    "ruby", "glyph_size", "ruby_size", "text_speed", "text_cursor", "text_offset",
    "clear_text", "end_case", "text_flag_1", "line_feed", "allow_skip", "talk_sounds",
    "bgm", "bgm_stop", "se", "se_stop", "nop", "unload_knocker", "unload_knocker_2",
    "partner_path_stop",
)))
# Which argument is a string index (the say text, ruby text, nameplates).
EVT_STRING_ARGS = {0x21: 0, 0x36: 0, 0x34: 1, 0x35: 1}
EVT_STRING_OPS = set(EVT_STRING_ARGS)


def evt_opcode_argc(program):
    """Argument counts read from the executable, checked against the copy here."""
    table = program.read(EVT_OPCODE_TABLE, EVT_OPCODE_STRIDE * len(EVT_OPCODE_ARGC))
    argc = tuple(struct.unpack_from("<I", table, i * EVT_OPCODE_STRIDE + 8)[0]
                 for i in range(len(EVT_OPCODE_ARGC)))
    if argc != EVT_OPCODE_ARGC:
        raise ValueError("EVT opcode table differs from the PAL release")
    return argc


def read_evt(data):
    """EVT: u32 n, u32 offsets[n+1]; offsets[0] = u16 bytecode, rest strings."""
    count = struct.unpack_from("<I", data, 0)[0]
    offsets = struct.unpack_from("<%dI" % (count + 1), data, 4)
    limit = offsets[1] if count else len(data)
    code, at = [], offsets[0]
    while True:
        if at + 2 > limit:
            raise ValueError("EVT bytecode runs into the string table")
        op = struct.unpack_from("<H", data, at)[0]
        if op >= len(EVT_OPCODE_ARGC):
            raise ValueError("Unknown EVT opcode 0x%02x at 0x%x" % (op, at))
        argc = EVT_OPCODE_ARGC[op]
        args = struct.unpack_from("<%dH" % argc, data, at + 2)
        code.append((at, op, args))
        at += 2 + 2 * argc
        if op == 0:
            break
    strings = [glyph_text(_units_at(data, o, len(data))) for o in offsets[1:]]
    return {"code": code, "code_end": at, "strings": strings}


def disassemble_evt(script):
    lines = []
    for at, op, args in script["code"]:
        name = EVT_OPCODE_NAMES.get(op, "op_%02x" % op)
        text = " ".join(str(a) for a in args)
        index = args[EVT_STRING_ARGS[op]] if op in EVT_STRING_ARGS else -1
        if 0 <= index < len(script["strings"]):
            text += "  ; " + script["strings"][index].replace("\n", "\\n")
        lines.append("%04x  %02x %-15s %s" % (at, op, name, text))
    return lines


# --- Executable campaign tables ------------------------------------------

STORY_EVENT_TABLE, STORY_EVENT_COUNT, STORY_EVENT_STRIDE = 0x335E80, 154, 28
REGULATION_TABLE, REGULATION_COUNT, REGULATION_STRIDE = 0x4227E0, 32, 36
MATCH_TABLE, MATCH_COUNT, MATCH_STRIDE = 0x41ECE0, 157, 56
CITY_DAY_TABLE, CITY_DAY_STRIDE = 0x422CE0, 60
DAY_TABLE, DAY_STRIDE = 0x42D0F0, 24
FINALS_FALLBACK_TABLE = 0x334A80  # per city: (day, night) match id
SCHEDULE_DAYS = 100
STAGES = 5  # save+0xA21
CITIES = ("Tokyo", "Stuttgart", "New York", "London", "Hong Kong", "Cairo", "Las Vegas")
FINAL_CITIES = (0, 3, 2)  # 3rd-place, 2nd-place and final match hosts
NO_MATCH = -1
ANY_MAKER, ANY_LEGS, UNUSED = 10, 11, -1
LEG_CLASSES = {11: "any", 12: "legged", 13: "wheeled", 14: "caterpillar/hover"}
SPECIAL_RULES = {
    -1: None, 0: "rental only", 1: "no optical camouflage", 2: "shoulder weapons only",
    3: "arm weapons only", 4: "close-range weapons only", 5: "lock-on weapons only",
    6: "grenade weapons only", 7: "E-Chip only",
}

# Code words that fix the schedule table geometry (f_2572c0 / f_257350).
_CITY_DAY_CODE = (0x2572C0, (0x00041100, 0x00441823, 0x3C020042, 0x00031880,
                             0x24422CE0, 0x03E00008, 0x00431021))
_DAY_CODE = (0x257350, (0x24020064, 0x0082001B, 0, 0, 0x00002010, 0x3C020043,
                        0x2442D0F0, 0x00041840, 0x00641821, 0x000318C0))
_EVENT_GETTER = (0x26A920, (0x2882009A, 0x14400003, 0x000418C0, 0x10000006,
                            0x0000102D, 0x3C020033, 0x00641823, 0x24425E80))


def _guard(program, guard):
    va, words = guard
    if struct.unpack("<%dI" % len(words), program.read(va, 4 * len(words))) != words:
        raise ValueError("Code at 0x%x differs from the PAL release" % va)


def _script_name(program, va):
    """Rows 146-151 point at small sdata records, not names; those give ''."""
    data = program.read(va, 32).split(b"\0")[0] if va else b""
    name = data.decode("latin1")
    return name if re.fullmatch(r"[a-z][a-z0-9_]+", name) else ""


def story_events(program):
    """154 town story/news/sponsor event triggers (getter 0x26A920)."""
    _guard(program, _EVENT_GETTER)
    rows = []
    for i in range(STORY_EVENT_COUNT):
        r = program.read(STORY_EVENT_TABLE + i * STORY_EVENT_STRIDE, STORY_EVENT_STRIDE)
        (name, condition, trigger, flags, place, sub_place, time_slot, threshold,
         condition_arg, required, blocking, done, unknown) = struct.unpack(
            "<IIBxHBBBxHHhhhH", r)
        rows.append({
            "index": i, "name": _script_name(program, name),
            "condition_fn": condition, "trigger": trigger, "flags": flags,
            "place": place, "sub_place": sub_place, "save_a20": time_slot,
            "progress_threshold": threshold, "condition_arg": condition_arg,
            "required_flag": required, "blocking_flag": blocking,
            "completion_flag": done, "unknown_1a": unknown,
        })
    return rows


def regulations(program):
    """32 rumble regulations, indexed like UK_RUMBLETYPENAME.MSG."""
    rows = []
    for i in range(REGULATION_COUNT):
        r = program.read(REGULATION_TABLE + i * REGULATION_STRIDE, REGULATION_STRIDE)
        rid, entry, maker, l0, l1, l2, l3, special, param, prize = struct.unpack("<8iHH", r)
        if rid != i:
            raise ValueError("Regulation table row %d is out of order" % i)
        rows.append({
            "id": rid, "entry_requirement": entry,
            "maker": None if maker == ANY_MAKER else maker,
            "leg_classes": [LEG_CLASSES[c] for c in (l0, l1, l2, l3) if c != UNUSED],
            "special": SPECIAL_RULES[special], "param_20": param, "prize": prize,
        })
    return rows


def matches(program):
    """157 match templates; save_record indexes the 15-byte save records."""
    rows = []
    for i in range(MATCH_COUNT):
        r = program.read(MATCH_TABLE + i * MATCH_STRIDE, MATCH_STRIDE)
        reg, night, low, high, level, record, record_b, pad = struct.unpack_from("<8h", r)
        if not 0 <= reg < REGULATION_COUNT or night not in (0, 1) or pad:
            raise ValueError("Match table row %d is not a match template" % i)
        rows.append({
            "id": i, "regulation": reg, "night": bool(night),
            "robots": (low, min(high, 6)), "level": level,
            "save_record": record, "save_record_b": record_b,
            "weights": list(struct.unpack_from("<10f", r, 16)),
        })
    return rows


def schedule(program):
    """The 100-day rota: per day, per city, per stage, (day, night) match ids."""
    _guard(program, _CITY_DAY_CODE)
    _guard(program, _DAY_CODE)
    days = []
    for day in range(SCHEDULE_DAYS):
        r = program.read(DAY_TABLE + day * DAY_STRIDE, DAY_STRIDE)
        header, patterns = struct.unpack_from("<5H", r), struct.unpack_from("<7H", r, 10)
        cities = []
        for pattern in patterns:
            row = program.read(CITY_DAY_TABLE + pattern * CITY_DAY_STRIDE, CITY_DAY_STRIDE)
            ids = struct.unpack_from("<%di" % (2 * STAGES), row)
            cities.append([ids[2 * s:2 * s + 2] for s in range(STAGES)])
        days.append({"day": day, "header": list(header), "cities": cities})
    fallback = struct.unpack("<14i", program.read(FINALS_FALLBACK_TABLE, 56))
    return {"days": days, "finals_fallback": [fallback[2 * c:2 * c + 2] for c in range(7)]}


def scheduled_match(rota, day, city, stage, night, finals_cleared=0):
    """Mirror of 0x257070: today's match id for a city slot, or NO_MATCH."""
    slot = int(bool(night))
    match = rota["days"][day % SCHEDULE_DAYS]["cities"][city][stage][slot]
    if stage != 3:
        return match
    host = FINAL_CITIES[finals_cleared] if finals_cleared < 3 else None
    if finals_cleared == 2:
        if city == host:
            return NO_MATCH if slot else 4
        if match < 0:
            return match
        if match in (6, 3):
            return 0x88 if slot else 0x86
        if match in (5, 2):
            return 0x87 if slot else 0x85
        return match
    if match < 0:
        return match
    if finals_cleared in (0, 1):
        if city == host:
            return (5, 6)[finals_cleared] if slot else (2, 3)[finals_cleared]
        allowed = (6, 5, 3, 2) if finals_cleared == 0 else (6, 3)
        if match in allowed:
            return rota["finals_fallback"][city][slot]
    return match


def next_stage(stage, ranker_cleared, regional_cleared, finals_cleared, tutorial_done):
    """Mirror of 0x25A4E0's stage advance (save+0xA21) after a battle."""
    if stage == 0 and tutorial_done:
        return 1
    if stage == 1 and ranker_cleared > 0x37:
        return 2
    if stage == 2 and regional_cleared == 7:
        return 3
    if stage == 3 and finals_cleared == 3:
        return 4
    return stage
