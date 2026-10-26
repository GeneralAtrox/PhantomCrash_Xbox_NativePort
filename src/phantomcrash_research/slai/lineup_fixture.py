"""Expected values for the port's native S.L.A.I. line-up tests.

The port reimplements 0x2225A0 and 0x223420 in C++ (slai_lineup.cpp) over
the opponents tables/slai_rumble.json carries. This writes what
opponent_rules.py's independent mirrors give for sample matches and save
states: the candidate lists, and the line-up three scripted random
sequences draw, so the native tests compare against the disc rather than
against the table they load.

    python -m phantomcrash_research.slai.lineup_fixture SLAI.iso port/tests/slai_lineup_expected.json
"""

from pathlib import Path
import json
import random
import sys

from .archive import SlaiDisc
from .opponent_rules import LISTS, Rules, match, pick_lineup
from .program import ProgramData

FIXTURE_VERSION = 1
# Rumbles of every kind: open (0), cameos sorted as their own (8, regulation
# 5), makers (9), CPU (17) and option (19) rules, cameos (42, 43), record
# 0x7D's roster (126), a leader from the start (130), regulation 30 (140,
# 141), and the first head-to-head.
SAMPLE_MATCHES = (0, 8, 9, 17, 19, 42, 43, 126, 130, 140, 141)
# Record bytes (cleared, 14 beaten flags) for the match's records: nothing,
# the first five beaten, all fourteen, and cleared.
SAVE_STATES = (bytes(15), bytes([0] + [1] * 5 + [0] * 9), bytes([0] + [1] * 14),
               bytes([1] + [1] * 14))
DRAWS = 16  # random values a line-up may take at most (5 slots x 3)


def _sequences(seed):
    rng = random.Random(seed)
    return ([round(rng.random(), 6) for _ in range(DRAWS)], [0.001] * DRAWS, [0.999] * DRAWS)


class _Scripted:
    def __init__(self, values):
        self.values = list(values)
        self.used = 0

    def random(self):
        value = self.values[self.used]
        self.used += 1
        return value


def _entries(entries):
    return [[e["opponent"], e["record"], e["flag"]] for e in entries]


def lineup_fixture(program):
    rules = Rules(program)
    head_to_head = next(i for i in range(157) if match(program, i)["robots_max"] == 2
                        and match(program, i)["record"] >= 0)
    cases = []
    for match_id in (*SAMPLE_MATCHES, head_to_head):
        m = match(program, match_id)
        for state_index, state in enumerate(SAVE_STATES):
            records = {rid: state for rid in (m["record"], m["record_b"]) if rid >= 0}
            found = rules.candidates(match_id, records)
            draws = []
            for values in _sequences(match_id * 16 + state_index):
                rng = _Scripted(values)
                lineup = pick_lineup(found, rng)
                draws.append({"values": values, "used": rng.used,
                              "lineup": [[e["slot"], LISTS.index(e["list"]), e["opponent"],
                                          e["record"], e["flag"]] for e in lineup]})
            cases.append({"match": match_id, "records": list(records), "state": list(state),
                          "status": found["status"],
                          "lists": [_entries(found["lists"][name]) for name in LISTS],
                          "draws": draws})
    return {
        "version": FIXTURE_VERSION,
        "source": "opponent_rules.py against SLES_529.40",
        "lists": list(LISTS),
        "layout": {"lists": "[list][entry] -> [opponent, record, flag]",
                   "lineup": "[slot, list, opponent, record, flag]"},
        "cases": cases,
    }


def write_fixture(image: Path, output: Path):
    with SlaiDisc(image) as disc:
        fixture = lineup_fixture(ProgramData(disc))
    output.write_text(json.dumps(fixture, separators=(",", ":")) + "\n", encoding="utf-8",
                      newline="\n")
    return fixture


if __name__ == "__main__":
    write_fixture(Path(sys.argv[1]), Path(sys.argv[2]))
