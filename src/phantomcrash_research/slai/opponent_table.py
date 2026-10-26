"""S.L.A.I.'s campaign opponents as the port fights them: the "opponents"
block of tables/slai_rumble.json.

Every opponent the rosters and the substitutions name is written with its
robot already resolved to the port's imported parts (native_parts.json), in
the garage's robot format, and with the facts the opponent check (f_222A60)
reads. The rosters, the substitutions and the leader rules (f_222F70) go
along, so the port can build the candidate lists (f_2225A0) and draw a
line-up (f_223420) itself; opponent_rules.py mirrors the same code.

Not decoded, so kept as source values and left to the port's data
(data/world/slai_rumble.json "opponents"): the CPU row (an AI chip), the
behaviour value, the 16-region paint and the emblem.

part_facts gives the "parts" block: what the player's entry check (f_259A40)
reads of each imported part, by the port's part id, so a robot fitted with
them answers it as S.L.A.I.'s own would.
"""

import struct

from .opponent_export import OpponentCatalogue
from .opponent_rules import SPECIAL_RECORD
from .opponents import OPPONENTS, OPTIONS, ROSTERS, _row, opponent, robot, roster

# S.L.A.I.'s weapon slots (f_222F30 order: arms 0-1, shoulders 2-3) as the
# port's robot part slots; the first arm is the right hand, as TEST DRIVE
# fits it.
WEAPON_SLOTS = (("arms", 0, "right_arm"), ("arms", 1, "left_arm"),
                ("shoulders", 0, "right_shoulder"), ("shoulders", 1, "left_shoulder"))
WEAPON_CLASS = 0x134  # f_222A60's close-range and grenade test (s16)
AIM_KIND = 0x18  # its lock-on test (u8)
OPTION_POINTS = 0x1A  # its no-optical-camouflage test: the options' total (s16)
EMBLEM_NONE = 205  # the port's robot without a sticker (garage.hpp emblem_none)


def _robot_record(catalogue, build, name):
    parts = {}
    for key, slot in (("body", "body"), ("legs", "legs")):
        if build[key] is not None and build[key]["port_part"] is not None:
            parts[slot] = {"id": build[key]["port_part"], "level": build[key]["level"]}
    for key, index, slot in WEAPON_SLOTS:
        part = build[key][index]
        if part is not None and part["port_part"] is not None:
            parts[slot] = {"id": part["port_part"], "level": part["level"]}
    return {"name": name or "", "parts": parts, "modules": [], "paint": 0, "emblem": EMBLEM_NONE}


def _check(rules, program, raw):
    """What f_222A60 reads of an opponent's build."""
    weapons = []
    for slot, part in enumerate(raw["arms"] + raw["shoulders"]):
        if part is None:
            weapons.append(None)
            continue
        record = rules._weapon(slot, part["id"])
        weapons.append({"class": struct.unpack_from("<h", record, WEAPON_CLASS)[0],
                        "aim": record[AIM_KIND]})
    points = sum(struct.unpack_from("<h", _row(program, OPTIONS, o), OPTION_POINTS)[0]
                 for o in raw["options"])
    return {"maker": rules.body_maker(raw), "leg_type": rules.leg_type(raw),
            "arms": weapons[:2], "shoulders": weapons[2:], "option_points": points,
            "cpu": raw["cpu"]}


def part_facts(rules, registration):
    """The "parts" block: each imported body's maker, legs' leg type and
    weapon's class and aim kind, by port part id (native_parts.json)."""
    facts = {"bodies": [], "legs": [], "arm_weapons": [], "shoulder_weapons": []}
    for entry in registration["parts"]:
        record, part, category = entry["source_record"], entry["part"], entry["category"]
        if part is None or record is None:
            continue
        if category == "body":
            facts["bodies"].append({"part": part,
                                    "maker": rules.body_maker({"body": {"id": record}})})
        elif category == "leg":
            facts["legs"].append({"part": part,
                                  "leg_type": rules.leg_type({"legs": {"id": record}})})
        elif category in ("arm", "shoulder"):
            raw = rules._weapon(0 if category == "arm" else 2, record)
            facts[category + "_weapons"].append(
                {"part": part, "class": struct.unpack_from("<h", raw, WEAPON_CLASS)[0],
                 "aim": raw[AIM_KIND]})
    for rows in facts.values():
        rows.sort(key=lambda row: row["part"])
    return facts


def _opponent(catalogue, opponent_id):
    program = catalogue.program
    row = opponent(program, opponent_id)
    detail = catalogue.opponent(opponent_id)
    raw = robot(program, row["robot"])
    pilot = detail["pilot"]
    return {
        "id": opponent_id,
        "pilot": pilot["name"] or "",
        "second_name": pilot["second_name"] or "",
        "level": row["level"],
        "behaviour": row["behaviour"],
        "robot": _robot_record(catalogue, detail, pilot["second_name"] or pilot["name"]),
        "check": _check(catalogue.rules, program, raw),
        # Undecoded source values (the 16-region paint stays in
        # opponent_export.py's research export).
        "source": {"robot": row["robot"], "cpu": detail["cpu"], "options": raw["options"],
                   "emblem": raw["emblem"]},
    }


def opponent_table(disc, registration):
    """The "opponents" block, read from the disc with the port's part
    registration (mods/slai/native_parts.json)."""
    catalogue = OpponentCatalogue(disc, registration)
    program = catalogue.program
    rules = catalogue.rules
    rosters = [roster(program, i) for i in range(ROSTERS[1])]
    used = sorted({i for row in rosters for i in [row["leader"], *row["members"]] if i is not None}
                  | {s["replacement"] for s in rules.substitutions}
                  | {i for s in rules.substitutions for i in s["ids"] if i >= 0})
    opponents = [_opponent(catalogue, i) for i in used if 0 <= i < OPPONENTS[1]]
    return {
        "rosters": [{"leader": -1 if row["leader"] is None else row["leader"],
                     "members": [-1 if m is None else m for m in row["members"]]}
                    for row in rosters],
        # f_2225A0 reads this roster for record 0x7D, not the record's own.
        "record_rosters": [{"record": SPECIAL_RECORD, "roster": rules.special_roster}],
        "substitutions": rules.substitutions,
        # f_222F70 by regulation: once every flag is set the leader joins
        # ("flags", given the manager's session check), on regulation 30's
        # counter ("counter") or never.
        "leader_rules": rules.completion_rules,
        "opponents": opponents,
        "provenance": {"opponents": hex(OPPONENTS[0]), "rosters": hex(ROSTERS[0]),
                       "check": "0x222a60", "candidates": "0x2225a0", "pick": "0x223420",
                       "leader_ready": "0x222f70", "substitution": "0x256cc0",
                       "robot_parts": "port parts through native_parts.json source_record"},
    }
