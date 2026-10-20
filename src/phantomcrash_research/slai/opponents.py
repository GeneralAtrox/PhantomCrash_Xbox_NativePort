"""Read-only decoders for the opponents S.L.A.I.'s campaign rumbles field.

An opponent id (the u16 values of the roster table) selects an 8-byte
opponent row: a pilot, a robot build, a behaviour value and a level. Pilots
name the opponent through a name-reference table into CHARANAME.MSG; robots
are the same 156-byte build the player's garage uses (body, legs, two arm and
two shoulder weapons with signed part levels, CPU chip, options, emblem).

All addresses are PAL SLES-52940 main-executable addresses, guarded below;
see docs/SLAI_OPPONENTS_RESEARCH.md. Field names are researcher-assigned
labels, not original source symbols. Selection rules are in opponent_rules.py.
"""

import struct

# (address, rows, stride) of each table and the accessor that indexes it.
NAME_REFS = (0x371A20, 2583, 4)  # f_22A070
PILOTS = (0x374280, 1272, 32)  # f_22A0A0
OPPONENTS = (0x37E600, 1969, 8)  # f_248780
ROBOTS = (0x382390, 1887, 156)  # f_2487A0, copied whole by f_20B970
ROSTERS = (0x420F40, 210, 30)  # f_2225A0, f_2240E0
CPUS = (0x3DF340, 452, 556)  # f_24D440
OPTIONS = (0x41C8F0, 108, 56)  # f_24D5E0
# The selectors' range checks (f_203470) are inclusive, so every part table
# has one row more than its upper bound: bodies 0..34, legs 0..89, ...
PART_TABLES = {
    "body": (0x3CA320, 35, 60, "UK_BODYNAME.MSG"),
    "leg": (0x3CAB60, 90, 208, "UK_LEGNAME.MSG"),
    "arm": (0x3CF480, 102, 312, "UK_ARMNAME.MSG"),
    "shoulder": (0x3D70D0, 107, 312, "UK_SHOULDERNAME.MSG"),
}
NAME_FILE = "CHARANAME.MSG"
CPU_NAME_FILE = "UK_CPUNAME.MSG"
OPTION_NAME_FILE = "UK_OPTIONNAME.MSG"
EMPTY = -1
ROSTER_MEMBERS = 14  # roster row: leader, then 14 members (save flags 1..14)

# Code words that fix the table geometry; a different release is refused.
_GUARDS = {
    0x248780: (0x3C020038, 0x000418C0, 0x2442E600, 0x03E00008, 0x00431021),
    0x2487A0: (0x00041080, 0x00441821, 0x000318C0, 0x3C020038, 0x00641823,
               0x24422390, 0x00031880, 0x03E00008, 0x00431021),
    0x22A070: (0x04810003, 0x3C020037, 0x10000004, 0x0000102D, 0x00041880,
               0x24421A20, 0x00431021),
    0x22A0A0: (0x04810003, 0x3C020037, 0x10000004, 0x0000102D, 0x00041940,
               0x24424280, 0x00431021),
    0x2227D4: (0x3C020042, 0x00031840, 0x24420F40, 0x0043B821),
    0x24D464: (0x3C02003E, 0x02031818, 0x2442F340),
    0x24D604: (0x3C020042, 0x00701823, 0x2442C8F0, 0x000318C0),
}


def guard(program):
    for va, words in _GUARDS.items():
        if struct.unpack("<%dI" % len(words), program.read(va, 4 * len(words))) != words:
            raise ValueError("S.L.A.I. code at 0x%x differs from the PAL release" % va)


def _row(program, table, index):
    va, rows, stride = table[:3]
    if not 0 <= index < rows:
        raise IndexError("Row %d outside table 0x%x" % (index, va))
    return program.read(va + index * stride, stride)


# --- names -------------------------------------------------------------------

def read_names(data):
    """CHARANAME-style MSG: u32 count, u32 offsets, 0xFFFF-terminated glyphs.
    Glyphs below 0x5F are ASCII minus 0x20; others stay as {xxxx} tokens."""
    count = struct.unpack_from("<I", data)[0]
    if 4 + 4 * count > len(data):
        raise ValueError("MSG offset table exceeds file")
    names = []
    for at in struct.unpack_from("<%dI" % count, data, 4):
        out = []
        while True:
            if at + 2 > len(data):
                raise ValueError("Unterminated MSG string")
            unit = struct.unpack_from("<H", data, at)[0]
            at += 2
            if unit == 0xFFFF:
                break
            out.append(chr(unit + 0x20) if unit < 0x5F else "{%04x}" % unit)
        names.append("".join(out))
    return names


def name_refs(program):
    """Name references: CHARANAME index (-1 none) and a second u16 (Low:
    perhaps a portrait or voice id)."""
    return [struct.unpack("<2h", _row(program, NAME_REFS, i)) for i in range(NAME_REFS[1])]


def resolve_name(refs, names, ref):
    if not 0 <= ref < len(refs):
        return None
    index = refs[ref][0]
    return names[index] if 0 <= index < len(names) else None


# --- pilots, robots, opponents -------------------------------------------

def _curve_u32(data, at):
    """Unsigned level curve evaluated by f_24CBC0: base, mid, top, knee, low_scale."""
    base, mid, top, knee, low_scale = struct.unpack_from("<3I2b", data, at)
    return {"base": base, "mid": mid, "top": top, "knee": knee, "low_scale": low_scale}


def parse_pilot(data):
    name, second_name, unknown_04 = struct.unpack_from("<3h", data)
    tail_18, tail_1a, tail_1c = struct.unpack_from("<3h", data, 0x18)
    return {"name_ref": name, "second_name_ref": second_name, "unknown_04": unknown_04,
            "level_curve": _curve_u32(data, 8), "unknown_18": tail_18,
            "unknown_1a": tail_1a, "unknown_1c": tail_1c}


def pilot(program, index):
    return {"id": index, **parse_pilot(_row(program, PILOTS, index))}


def parse_robot(data):
    """156-byte build. Slot order follows the accessors: f_222F10 / f_222F50
    read body (+0) and legs (+4), f_222F30 weapon slots 0..3 (+8..+0x14),
    where f_24D760 sends slots 0-1 to the arm table and 2-3 to shoulders.
    Each slot is (part id, signed part level -99..99); -1 is an empty slot."""
    slots = [struct.unpack_from("<2h", data, 4 * i) for i in range(6)]
    part = lambda p: None if p[0] == EMPTY else {"id": p[0], "level": p[1]}
    return {
        "body": part(slots[0]), "legs": part(slots[1]),
        "arms": [part(s) for s in slots[2:4]],
        "shoulders": [part(s) for s in slots[4:6]],
        "cpu": struct.unpack_from("<h", data, 0x18)[0],
        "paint_ids": list(struct.unpack_from("<16h", data, 0x1A)),
        "unknown_3a": list(struct.unpack_from("<3h", data, 0x3A)),
        "paint_values": [round(v, 6) for v in struct.unpack_from("<16f", data, 0x40)],
        "emblem": struct.unpack_from("<h", data, 0x80)[0],
        "unknown_82": list(struct.unpack_from("<2h", data, 0x82)),
        "options": [o for o in struct.unpack_from("<10h", data, 0x86) if o != EMPTY],
        "unknown_9a": struct.unpack_from("<h", data, 0x9A)[0],
    }


def robot(program, index):
    return {"id": index, **parse_robot(_row(program, ROBOTS, index))}


def parse_opponent(data):
    pilot_id, robot_id, behaviour, level = struct.unpack("<4h", data)
    return {"pilot": pilot_id, "robot": robot_id, "behaviour": behaviour, "level": level}


def opponent(program, index):
    return {"id": index, **parse_opponent(_row(program, OPPONENTS, index))}


def roster(program, index):
    """Roster row: [0] leader (joins once the record is complete), [1..14]
    members whose beaten flags are save record bytes 1..14. 0xFFFF = none."""
    ids = [None if v == 0xFFFF else v for v in struct.unpack("<15H", _row(program, ROSTERS, index))]
    return {"id": index, "leader": ids[0], "members": ids[1:]}


# --- part tables and the port's imported parts ---------------------------

def part_record(program, category, index):
    data = _row(program, PART_TABLES[category], index)
    name, maker = struct.unpack_from("<2H", data)
    if category == "body":
        model = name  # body variants 10..34 repeat bodies 0..9 (same name and maker)
    elif category == "leg":
        model = struct.unpack_from("<H", data, 4)[0]
    else:
        model = struct.unpack_from("<I", data, 4)[0]
    return {"name_index": name, "maker": maker, "model": model}


def port_part_map(program, registration):
    """(category, S.L.A.I. record) -> port part index for every record.

    The importer registers one port part per shipped model (native_parts.py
    source_records); the other records sharing that model are variants, so
    they resolve to the same port part. registration is the content of
    mods/slai/native_parts.json."""
    by_model = {}
    for entry in registration["parts"]:
        category = entry["category"]
        key = part_record(program, category, entry["source_record"])["model"]
        by_model.setdefault((category, key), entry["part"])
    result = {}
    for category, table in PART_TABLES.items():
        for index in range(table[1]):
            part = by_model.get((category, part_record(program, category, index)["model"]))
            if part is not None:
                result[(category, index)] = part
    return result


def cpu_name_index(program, index):
    return struct.unpack_from("<h", _row(program, CPUS, index))[0]


def option_name_index(program, index):
    return struct.unpack_from("<h", _row(program, OPTIONS, index))[0]
