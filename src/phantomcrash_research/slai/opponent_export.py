"""Resolve S.L.A.I. campaign opponents into names, builds and port parts.

Combines opponents.py (tables) and opponent_rules.py (selection) with the
disc's name files and, when given, the port's part registration
(mods/slai/native_parts.json). Output is plain JSON-ready data for the
planned slai_rumble.json export; nothing here writes into the port.
"""

import json
from pathlib import Path

from .archive import SlaiDisc
from .music import messages
from .opponent_rules import Rules, match
from .opponents import (CPU_NAME_FILE, NAME_FILE, OPPONENTS, OPTION_NAME_FILE, PART_TABLES,
                        PILOTS, ROSTERS, cpu_name_index, guard, name_refs, opponent,
                        option_name_index, part_record, pilot, port_part_map,
                        read_names, resolve_name, robot, roster)
from .program import ProgramData

PROVENANCE = {
    "executable": "SLES_529.40 (PAL)",
    "names": "shipped CHARANAME.MSG and UK_*NAME.MSG text",
    "field_labels": "researcher assigned",
    "tables": {"name_refs": "0x371A20", "pilots": "0x374280", "opponents": "0x37E600",
               "robots": "0x382390", "rosters": "0x420F40", "substitutions": "0x422C60"},
}


def _msg(disc, name, reader):
    found = disc.find(name)
    if not found:
        raise ValueError("Missing %s" % name)
    return reader(disc.read(found[0]))


class OpponentCatalogue:
    """Everything needed to describe opponents, read once from the disc."""

    def __init__(self, disc, registration=None):
        self.program = ProgramData(disc)
        guard(self.program)
        self.rules = Rules(self.program)
        self.names = _msg(disc, NAME_FILE, read_names)
        self.refs = name_refs(self.program)
        self.part_names = {c: _msg(disc, t[3], messages) for c, t in PART_TABLES.items()}
        self.cpu_names = _msg(disc, CPU_NAME_FILE, messages)
        self.option_names = _msg(disc, OPTION_NAME_FILE, messages)
        self.port = port_part_map(self.program, registration) if registration else {}

    def part(self, category, slot):
        if slot is None:
            return None
        record = part_record(self.program, category, slot["id"])
        names = self.part_names[category]
        index = record["name_index"]
        return {"id": slot["id"], "level": slot["level"],
                "name": names[index] if index < len(names) else None,
                "maker": record["maker"], "port_part": self.port.get((category, slot["id"]))}

    def build(self, robot_id):
        r = robot(self.program, robot_id)
        cpu_name = cpu_name_index(self.program, r["cpu"]) if r["cpu"] >= 0 else -1
        return {
            "robot": robot_id,
            "body": self.part("body", r["body"]), "legs": self.part("leg", r["legs"]),
            "arms": [self.part("arm", s) for s in r["arms"]],
            "shoulders": [self.part("shoulder", s) for s in r["shoulders"]],
            "cpu": {"id": r["cpu"], "name": self.cpu_names[cpu_name]
                    if 0 <= cpu_name < len(self.cpu_names) else None},
            "options": [{"id": o, "name": self._option_name(o)} for o in r["options"]],
            "emblem": r["emblem"],
            "raw": {k: r[k] for k in ("paint_ids", "paint_values", "unknown_3a",
                                      "unknown_82", "unknown_9a")},
        }

    def _option_name(self, index):
        name = option_name_index(self.program, index)
        return self.option_names[name] if 0 <= name < len(self.option_names) else None

    def opponent(self, opponent_id):
        o = opponent(self.program, opponent_id)
        p = pilot(self.program, o["pilot"])
        return {
            "id": opponent_id, "level": o["level"], "behaviour": o["behaviour"],
            "pilot": {"id": o["pilot"],
                      "name": resolve_name(self.refs, self.names, p["name_ref"]),
                      "second_name": resolve_name(self.refs, self.names, p["second_name_ref"]),
                      **{k: p[k] for k in ("level_curve", "unknown_04", "unknown_18",
                                           "unknown_1a", "unknown_1c")}},
            **self.build(o["robot"]),
        }

    def roster(self, index):
        row = roster(self.program, index)
        return {"id": index, "leader": row["leader"], "members": row["members"]}

    def match_opponents(self, match_id, records=None, flags=frozenset(), session_ok=True):
        """A match's candidate lists with every opponent fully described."""
        found = self.rules.candidates(match_id, records, flags, session_ok)
        lists = {name: [{**entry, "detail": self.opponent(entry["opponent"])}
                        for entry in entries] for name, entries in found["lists"].items()}
        return {"match": found["match"], "status": found["status"], "lists": lists}


def read_campaign_opponents(disc, registration=None):
    """All opponent tables, with every referenced opponent described."""
    catalogue = OpponentCatalogue(disc, registration)
    program = catalogue.program
    rows = [opponent(program, i) for i in range(OPPONENTS[1])]
    used = sorted({i for r in range(ROSTERS[1]) for i in
                   [roster(program, r)["leader"], *roster(program, r)["members"]] if i is not None}
                  | {s["replacement"] for s in catalogue.rules.substitutions})
    return {
        "provenance": PROVENANCE,
        "rosters": [catalogue.roster(i) for i in range(ROSTERS[1])],
        "substitutions": catalogue.rules.substitutions,
        "opponents": {i: catalogue.opponent(i) for i in used},
        "opponent_rows": len(rows), "pilot_rows": PILOTS[1],
        "matches": [match(program, i) for i in range(157)],
    }


def export_opponents(image, output, registration_path=None):
    registration = None
    if registration_path and Path(registration_path).exists():
        registration = json.loads(Path(registration_path).read_text(encoding="utf-8"))
    with SlaiDisc(Path(image)) as disc:
        data = read_campaign_opponents(disc, registration)
    Path(output).write_text(json.dumps(data, indent=1), encoding="utf-8")
    return {"opponents": len(data["opponents"]), "rosters": len(data["rosters"])}
