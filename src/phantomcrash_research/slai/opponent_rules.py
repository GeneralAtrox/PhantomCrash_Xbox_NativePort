"""Mirrors of how S.L.A.I. turns a match template into opponents.

- f_256CC0: story-flag substitution of opponent ids;
- f_222A60: the opponent eligibility check against the match's regulation;
- f_2225A0: the candidate lists built by the rumble constructor f_222070;
- f_223420: the per-slot pick made by f_2232E0 for slots 1..5.

PAL SLES-52940 addresses; labels are researcher-assigned. Pure functions
take already-read rows so the port's native tests can share expected values.
"""

import struct

from .opponents import EMPTY, OPTIONS, ROSTER_MEMBERS, _row, opponent, robot, roster

SUBSTITUTIONS = (0x422C60, 6, 16)  # {replacement, 7 ids}
# f_256CC0's jump table (0x422CC0): replace while (set flags, clear flags).
SUBSTITUTION_FLAGS = (((), (0x16,)), ((0x16,), (0x24,)), ((0x24,), (0x25,)),
                      ((), (0x25,)), ((), (0x25,)), ((), (0x25,)))
REGULATIONS = (0x4227E0, 32, 36)
MATCHES = (0x41ECE0, 157, 56)
LEG_TYPE_TABLE = 0x41EA00  # f_24D7D0: leg byte (+6) 0..34 -> leg type 0..10
STATUS_TABLE = 0x371450  # f_2225A0: record status by regulation (31 rows)
COMPLETION_TABLE = 0x3714F0  # f_222F70: leader-ready rule by regulation
SPECIAL_ROSTER = 0x30A090  # roster used for record 0x7D (first entry 125)
SPECIAL_RECORD = 0x7D
ANY_MAKER, ANY_LEGS = 10, 11
LEG_CLASS_TYPES = {12: range(0, 3), 13: range(3, 7), 14: range(7, 11)}
ALWAYS_ELIGIBLE_MAKERS = (8, 9)
POOL_LIMIT, PICK_WINDOW = 2, 3
LEADER_SLOTS_ONLY = 4  # slots below 4 take roster opponents, 4..5 the pool
CAMEO_CHANCE, UNBEATEN_CHANCE = 1 / 64, 0.95  # 0x3C800000, 0x3F733333
BEATEN_THRESHOLD = 4  # beaten entries needed before the 5% beaten draw
MAIN_LIMIT = 4  # roster draws stop at 4 (plus the manager's +6 count, 0)
LISTS = ("unbeaten", "cameo", "leader", "beaten", "pool")

_STATUS_TARGETS = {0x2226E8: "flags5", 0x222740: "flags14", 0x2227A8: "open",
                   0x222798: "status2"}
_COMPLETION_TARGETS = {0x222FE0: "flags", 0x223028: "flags", 0x2230A0: "never",
                       0x223070: "counter"}


def _jump_rules(program, address, targets, rows=31):
    words = struct.unpack("<%dI" % rows, program.read(address, 4 * rows))
    if any(w not in targets for w in words):
        raise ValueError("Jump table 0x%x differs from the PAL release" % address)
    return [targets[w] for w in words]


def leg_types(program):
    """f_24D7D0's jump table: each case's delay slot loads the type."""
    words = struct.unpack("<35I", program.read(LEG_TYPE_TABLE, 140))
    types = []
    for target in words:
        instruction = struct.unpack("<I", program.read(target + 4, 4))[0]
        if instruction == 0x0000102D:  # move v0, zero
            types.append(0)
        elif instruction >> 16 == 0x2402:  # addiu v0, zero, n
            types.append(instruction & 0xFFFF)
        else:
            raise ValueError("Unexpected leg-type case at 0x%x" % target)
    return types


def substitutions(program):
    rows = []
    for i in range(SUBSTITUTIONS[1]):
        values = struct.unpack("<8h", _row(program, SUBSTITUTIONS, i))
        set_flags, clear_flags = SUBSTITUTION_FLAGS[i]
        rows.append({"replacement": values[0], "ids": list(values[1:]),
                     "while_set": list(set_flags), "while_clear": list(clear_flags)})
    return rows


def substitute(rows, opponent_id, flags=frozenset()):
    """Mirror of f_256CC0: the first row listing the id replaces it while
    its story flags hold (f_204E50); otherwise the id is unchanged."""
    for row in rows:
        if opponent_id in row["ids"]:
            if (all(f in flags for f in row["while_set"])
                    and not any(f in flags for f in row["while_clear"])):
                return row["replacement"]
            return opponent_id
    return opponent_id


def regulation(program, index):
    rid, entry, maker, l0, l1, l2, l3, special, param, prize = struct.unpack(
        "<8iHH", _row(program, REGULATIONS, index))
    return {"id": rid, "maker": maker, "leg_classes": [l0, l1, l2, l3], "special": special}


def match(program, index):
    reg, night, low, high, pool, record, record_b, _ = struct.unpack_from(
        "<8h", _row(program, MATCHES, index))
    return {"id": index, "regulation": reg, "night": bool(night), "robots_min": low,
            "robots_max": min(high, 6), "pool": pool, "record": record, "record_b": record_b}


class Rules:
    """Executable data the checks need, read once."""

    def __init__(self, program):
        self.program = program
        self.leg_types = leg_types(program)
        self.status_rules = _jump_rules(program, STATUS_TABLE, _STATUS_TARGETS)
        self.completion_rules = _jump_rules(program, COMPLETION_TABLE, _COMPLETION_TARGETS)
        self.substitutions = substitutions(program)
        self.special_roster = struct.unpack("<h", program.read(SPECIAL_ROSTER, 2))[0]

    def body_maker(self, build):
        body = build["body"]
        return 10 if body is None else self.program.read(
            0x3CA320 + 60 * body["id"] + 2, 1)[0]

    def leg_type(self, build):
        legs = build["legs"]
        if legs is None:
            return 11
        byte = struct.unpack("<h", self.program.read(0x3CAB60 + 208 * legs["id"] + 6, 2))[0] & 0xFF
        return self.leg_types[byte] if byte < len(self.leg_types) else 0

    def _weapon(self, slot, part_id):
        base, stride = (0x3CF480, 312) if slot < 2 else (0x3D70D0, 312)
        return self.program.read(base + stride * part_id, stride)

    def eligible(self, opponent_id, reg):
        """Mirror of f_222A60 for one opponent id and a regulation row."""
        if opponent_id is None or opponent_id == EMPTY:
            return False
        build = robot(self.program, opponent(self.program, opponent_id)["robot"])
        maker = self.body_maker(build)
        if maker in ALWAYS_ELIGIBLE_MAKERS:
            return True
        if reg["maker"] != ANY_MAKER and reg["maker"] != maker:
            return False
        kind = self.leg_type(build)
        legs_ok = False
        for i, leg_class in enumerate(reg["leg_classes"]):
            if leg_class == ANY_LEGS or kind in LEG_CLASS_TYPES.get(leg_class, ()) \
                    or (leg_class not in LEG_CLASS_TYPES and i == 0):
                legs_ok = True
                break
        if not legs_ok:
            return False
        return self._special_ok(build, reg["special"])

    def _special_ok(self, build, special):
        weapons = [(slot, part) for slot, part in enumerate(build["arms"] + build["shoulders"])]
        present = [(slot, part["id"]) for slot, part in weapons if part is not None]
        if special == 1:  # option +0x1A total must not be positive
            return sum(struct.unpack_from("<h", _row(self.program, OPTIONS, o), 0x1A)[0]
                       for o in build["options"]) <= 0
        if special == 2:
            return all(part is None for part in build["arms"])
        if special == 3:
            return all(part is None for part in build["shoulders"])
        if special in (4, 6):
            allowed = (2,) if special == 4 else (3, 15)
            return all(struct.unpack_from("<h", self._weapon(s, p), 0x134)[0] in allowed
                       for s, p in present)
        if special == 5:
            return all(self._weapon(s, p)[0x18] in (1, 9) for s, p in present)
        if special == 7:
            return build["cpu"] in (0, 1)
        return True  # -1 none, 0 rental only (a player-side rule)

    def status(self, reg_id, record):
        """f_2225A0's record status: 3 = leader open, 2 = all flags set."""
        rule = self.status_rules[reg_id] if 0 <= reg_id < len(self.status_rules) else "open"
        if rule == "open":
            return 3
        if rule == "status2":
            return 2
        if record[0] == 1:
            return 3
        flags = 5 if rule == "flags5" else ROSTER_MEMBERS
        return 2 if all(b == 1 for b in record[1:1 + flags]) else 0

    def leader_ready(self, reg_id, status, session_ok=True, counter_ok=False):
        """f_222F70. session_ok stands for the rumble manager's runtime check
        (f_224830() * f_2230C0() >= 2, unresolved); counter_ok for regulation
        30's counter (>= 5)."""
        if status == 3:
            return True
        rule = self.completion_rules[reg_id] if 0 <= reg_id < len(self.completion_rules) else "never"
        if rule == "flags":
            return status == 2 and session_ok
        return rule == "counter" and counter_ok

    def candidates(self, match_id, records=None, flags=frozenset(), session_ok=True):
        """Mirror of f_2225A0 (constructor arguments 0, 0): candidate lists.

        records maps a save record id to its 15 bytes (cleared, 14 beaten
        flags); missing records are all zero. Entries are dicts with the
        opponent id, the record and the flag byte (0 = the leader)."""
        records = records or {}
        get = lambda rid: list(records.get(rid, bytes(15)))
        m = match(self.program, match_id)
        reg = regulation(self.program, m["regulation"])
        lists = {name: [] for name in LISTS}
        status = 0
        for second, rid in enumerate((m["record"], m["record_b"])):
            if rid < 0:
                continue
            record = get(rid)
            if second == 0:
                status = self.status(m["regulation"], record)
            row = roster(self.program, self.special_roster if rid == SPECIAL_RECORD else rid)
            for i, member in enumerate(row["members"]):
                oid = substitute(self.substitutions, -1 if member is None else member, flags)
                if not self.eligible(oid, reg):
                    continue
                entry = {"opponent": oid, "record": rid, "flag": i + 1}
                beaten = record[i + 1] == 1
                if second and m["regulation"] != 5:
                    lists["cameo"].append(entry)
                else:
                    lists["beaten" if beaten else "unbeaten"].append(entry)
            if second == 0 and row["leader"] is not None:
                oid = substitute(self.substitutions, row["leader"], flags)
                if self.eligible(oid, reg) and self.leader_ready(m["regulation"], status, session_ok):
                    entry = {"opponent": oid, "record": rid, "flag": 0}
                    lists["beaten" if record[0] == 1 else "leader"].append(entry)
        if m["pool"] >= 0:
            pool = roster(self.program, m["pool"])
            for i, member in enumerate(pool["members"]):
                oid = substitute(self.substitutions, -1 if member is None else member, flags)
                if self.eligible(oid, reg):
                    lists["pool"].append({"opponent": oid, "record": m["pool"], "flag": i + 1})
        return {"match": m, "status": status, "lists": lists}


def pick_lineup(found, rng, leader_ready=True):
    """Mirror of f_2232E0/f_223420 for slots 1..5 (slot 0 is the player).

    rng.random() stands in for f_249C50's rand()/32768. Slots 1..3 draw from
    the roster lists until robots_min are taken; slots 4..5 draw from the
    pool, at most twice. Each draw takes a uniform index among the first
    three remaining entries, so roster order matters."""
    m = found["match"]
    lists = {name: list(entries) for name, entries in found["lists"].items()}
    taken = {name: 0 for name in LISTS}
    lineup = []
    for slot in range(1, 6):
        if sum(taken.values()) >= m["robots_max"]:
            break
        main_taken = sum(taken[n] for n in LISTS if n != "pool")
        use_pool = taken["pool"] < POOL_LIMIT and bool(lists["pool"])
        use_main = (main_taken < MAIN_LIMIT and main_taken < m["robots_min"]
                    and any(lists[n] for n in LISTS if n != "pool"))
        if slot < LEADER_SLOTS_ONLY:
            use_pool = False
        else:
            use_main = False
        source = None
        if use_main:
            if lists["leader"] and leader_ready:
                source = "leader"
            elif lists["cameo"] and rng.random() < CAMEO_CHANCE:
                source = "cameo"
            elif lists["unbeaten"]:
                source = "unbeaten" if len(lists["beaten"]) < BEATEN_THRESHOLD \
                    or rng.random() < UNBEATEN_CHANCE else "beaten"
            elif lists["beaten"]:
                source = "beaten"
        elif use_pool:
            source = "pool"
        if source is None:
            continue
        index = min(int(rng.random() * min(len(lists[source]), PICK_WINDOW)),
                    len(lists[source]) - 1)
        lineup.append({"slot": slot, "list": source, **lists[source].pop(index)})
        taken[source] += 1
    return lineup
