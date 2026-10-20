"""Expected values for the port's native S.L.A.I. Rumble tests.

The port reimplements 0x257070 and 0x25A4E0 in C++ (slai_rumble_rules.cpp)
and reads tables/slai_rumble.json. This writes a small fixture of what
campaign.py's independent mirrors give for sample days, every city, stage,
slot and finals count, plus spot checks of the decoded rows, so the native
tests compare against the disc rather than against the table they load.

    python -m phantomcrash_research.slai.rumble_fixture SLAI.iso port/tests/slai_rumble_expected.json
"""

from pathlib import Path
import json
import sys

from .archive import SlaiDisc
from .campaign import CITIES, STAGES, matches, next_stage, regulations, schedule, scheduled_match
from .program import ProgramData

FIXTURE_VERSION = 1
# Early, middle and late days, the last day and two that wrap past it.
SAMPLE_DAYS = (0, 1, 2, 17, 42, 57, 77, 99, 100, 163)
FINALS_COUNTS = range(4)
SAMPLE_MATCHES = (0, 2, 4, 5, 8, 42, 47, 78, 83, 125, 127, 133, 145, 156)
SAMPLE_REGULATIONS = (0, 2, 4, 5, 6, 10, 11, 13, 14, 15, 19, 22, 26, 27)
# (stage, ranker cleared, regional cleared, finals cleared, tutorial done)
LADDER_CASES = ((0, 0, 0, 0, False), (0, 0, 0, 0, True), (1, 55, 0, 0, True),
                (1, 56, 0, 0, True), (2, 56, 6, 0, True), (2, 56, 7, 0, True),
                (3, 56, 7, 2, True), (3, 56, 7, 3, True), (4, 56, 7, 3, True),
                (1, 56, 7, 3, True))


def rumble_fixture(program):
    rota = schedule(program)
    lookups = []
    for day in SAMPLE_DAYS:
        lookups.append([[[[scheduled_match(rota, day, city, stage, night, cleared)
                           for cleared in FINALS_COUNTS]
                          for night in (False, True)]
                         for stage in range(STAGES)]
                        for city in range(len(CITIES))])
    match_rows = {m["id"]: m for m in matches(program)}
    rule_rows = {r["id"]: r for r in regulations(program)}
    return {
        "version": FIXTURE_VERSION,
        "source": "campaign.py against SLES_529.40",
        "days": list(SAMPLE_DAYS),
        "lookups": lookups,
        "lookups_layout": "[day][city][stage][night][finals cleared] -> match id",
        "matches": [{"id": i, "regulation": match_rows[i]["regulation"],
                     "night": match_rows[i]["night"], "robots": list(match_rows[i]["robots"]),
                     "save_record": match_rows[i]["save_record"],
                     "save_record_b": match_rows[i]["save_record_b"]}
                    for i in SAMPLE_MATCHES],
        "regulations": [{"id": i, "entry_requirement": rule_rows[i]["entry_requirement"],
                         "maker": rule_rows[i]["maker"],
                         "leg_classes": rule_rows[i]["leg_classes"],
                         "special": rule_rows[i]["special"],
                         "param_20": rule_rows[i]["param_20"], "prize": rule_rows[i]["prize"]}
                        for i in SAMPLE_REGULATIONS],
        "ladder": [{"case": list(case), "next": next_stage(*case)} for case in LADDER_CASES],
    }


def write_fixture(image: Path, output: Path):
    with SlaiDisc(image) as disc:
        fixture = rumble_fixture(ProgramData(disc))
    output.write_text(json.dumps(fixture, separators=(",", ":")) + "\n", encoding="utf-8")
    return fixture


if __name__ == "__main__":
    write_fixture(Path(sys.argv[1]), Path(sys.argv[2]))
