"""Export S.L.A.I.'s Club Wired rumble tables as tables/slai_rumble.json.

The table holds what the S.L.A.I. Rumble mode schedules and fights: the 32
regulations, the 157 match templates, the 100-day rota with its 700 city-day
rows, and the finals injection of 0x257070 stated as data. Values come from
the decoders in campaign.py; see docs/SLAI_CAMPAIGN_RESEARCH.md for the
format and the confidence of each field.
"""

from pathlib import Path
import struct

from .archive import SlaiDisc
from .assets import save
from .campaign import (
    CITIES, CITY_DAY_STRIDE, CITY_DAY_TABLE, DAY_STRIDE, DAY_TABLE, FINAL_CITIES,
    FINALS_FALLBACK_TABLE, MATCH_COUNT, MATCH_TABLE, NO_MATCH, REGULATION_COUNT,
    REGULATION_TABLE, SCHEDULE_DAYS, STAGES, _CITY_DAY_CODE, _DAY_CODE, matches,
    read_msg, regulations, schedule,
)
from .program import ProgramData

TABLE_FILE = "tables/slai_rumble.json"
TABLE_VERSION = 1
NAMES_FILE = "UK_RUMBLETYPENAME.MSG"
MAKERS = ("Carro", "Proton", "Hartman", "KHT", "Zwerg")
DISPLAY_ORDER = (2, 6, 0, 4, 5, 1, 3)  # schedule screen byte order 0x43ACF0
FINALS_STAGE = 3
SHOP_LINEUPS_OFFSET, SHOP_LINEUPS = 0x28, 10
NO_LINEUP = 0xFFFF

# The finals of 0x257070 as data. "matches" are the host's (day, night) ids.
# Elsewhere, a scheduled id in "from" becomes "with" by slot, or the city's
# finals_fallback pair when "with" is None. A host slot needs a scheduled
# match unless "host_ignores_rota" is set.
FINAL_ROUNDS = (
    {"name": "3rd place", "matches": (2, 5), "host_ignores_rota": False,
     "replace": ({"from": (6, 5, 3, 2), "with": None},)},
    {"name": "2nd place", "matches": (3, 6), "host_ignores_rota": False,
     "replace": ({"from": (6, 3), "with": None},)},
    {"name": "final", "matches": (4, NO_MATCH), "host_ignores_rota": True,
     "replace": ({"from": (6, 3), "with": (0x86, 0x88)},
                 {"from": (5, 2), "with": (0x85, 0x87)})},
)


def _f32(value):
    """Shortest decimal that reads back as the same 32-bit float."""
    exact = struct.pack("<f", value)
    for digits in range(1, 10):
        text = "%.*g" % (digits, value)
        if struct.pack("<f", float(text)) == exact:
            return float(text)
    return value


def _regulations(program, names):
    rows = []
    for rule in regulations(program):
        maker = rule["maker"]
        rows.append({"id": rule["id"], "name": names[rule["id"]],
                     "entry_requirement": rule["entry_requirement"],
                     "maker": maker, "maker_name": None if maker is None else MAKERS[maker],
                     "leg_classes": rule["leg_classes"], "special": rule["special"],
                     "param_20": rule["param_20"], "prize": rule["prize"]})
    return rows


def _matches(program):
    rows = []
    for match in matches(program):
        low, high = match["robots"]
        rows.append({"id": match["id"], "regulation": match["regulation"],
                     "night": match["night"], "robots_min": low, "robots_max": high,
                     "level": match["level"], "save_record": match["save_record"],
                     "save_record_b": match["save_record_b"],
                     "weights": [_f32(w) for w in match["weights"]]})
    return rows


def _rota(program):
    rota = schedule(program)
    days = []
    for day in rota["days"]:
        row = program.read(DAY_TABLE + day["day"] * DAY_STRIDE, DAY_STRIDE)
        days.append({"day": day["day"], "shop_lineups": day["header"],
                     "city_days": list(struct.unpack_from("<7H", row, 10))})
    used = {pattern for day in days for pattern in day["city_days"]}
    city_days = []
    for pattern in range(max(used) + 1):
        row = program.read(CITY_DAY_TABLE + pattern * CITY_DAY_STRIDE, CITY_DAY_STRIDE)
        ids = struct.unpack_from("<%di" % (2 * STAGES), row)
        lineups = struct.unpack_from("<%dH" % SHOP_LINEUPS, row, SHOP_LINEUPS_OFFSET)
        city_days.append({"id": pattern,
                          "stages": [list(ids[2 * s:2 * s + 2]) for s in range(STAGES)],
                          "shop_lineups": [NO_MATCH if v == NO_LINEUP else v for v in lineups]})
    return {"days": days, "city_days": city_days}, rota["finals_fallback"]


def _finals(fallback):
    rounds = []
    for host, spec in zip(FINAL_CITIES, FINAL_ROUNDS):
        rounds.append({"name": spec["name"], "host": host, "host_name": CITIES[host],
                       "matches": list(spec["matches"]),
                       "host_ignores_rota": spec["host_ignores_rota"],
                       "replace": [{"from": list(r["from"]),
                                    "with": None if r["with"] is None else list(r["with"])}
                                   for r in spec["replace"]]})
    return {"stage": FINALS_STAGE, "rounds": rounds,
            "fallback": [list(pair) for pair in fallback]}


def rumble_table(program, names):
    """The slai_rumble.json document from a ProgramData and the regulation names."""
    if len(names) != REGULATION_COUNT:
        raise ValueError("%s has %d names, expected %d" % (NAMES_FILE, len(names),
                                                           REGULATION_COUNT))
    rota, fallback = _rota(program)
    hexes = lambda *values: [hex(v) for v in values]
    return {
        "version": TABLE_VERSION,
        "cities": [{"id": i, "name": name} for i, name in enumerate(CITIES)],
        "display_order": list(DISPLAY_ORDER),
        "schedule_days": SCHEDULE_DAYS, "stages": STAGES,
        "regulations": _regulations(program, names),
        "matches": _matches(program),
        "rota": rota,
        "finals": _finals(fallback),
        "provenance": {
            "disc": "SLES_529.40", "names": NAMES_FILE,
            "tables": {"regulations": hex(REGULATION_TABLE), "matches": hex(MATCH_TABLE),
                       "days": hex(DAY_TABLE), "city_days": hex(CITY_DAY_TABLE),
                       "finals_fallback": hex(FINALS_FALLBACK_TABLE)},
            "guards": {hex(_CITY_DAY_CODE[0]): hexes(*_CITY_DAY_CODE[1]),
                       hex(_DAY_CODE[0]): hexes(*_DAY_CODE[1])},
            "checks": ["regulation id equals its row",
                       "every match has a valid regulation and a day/night value",
                       "%d match rows" % MATCH_COUNT],
            "logic": {"rota_lookup": "0x257070", "stage_advance": "0x25a4e0",
                      "entry_check": "0x259a40", "opponent_check": "0x222a60",
                      "status": "0x223ed0"},
        },
    }


def table_match(table, day, city, stage, night, finals_cleared=0):
    """Today's match id read from the exported table; equals scheduled_match."""
    slot = int(bool(night))
    rota, finals = table["rota"], table["finals"]
    pattern = rota["days"][day % table["schedule_days"]]["city_days"][city]
    match = rota["city_days"][pattern]["stages"][stage][slot]
    if stage != finals["stage"] or finals_cleared >= len(finals["rounds"]):
        return match
    final = finals["rounds"][finals_cleared]
    if city == final["host"] and (final["host_ignores_rota"] or match >= 0):
        return final["matches"][slot]
    if match < 0:
        return match
    for rule in final["replace"]:
        if match in rule["from"]:
            return (finals["fallback"][city] if rule["with"] is None else rule["with"])[slot]
    return match


def register_rumble(image: Path, mods: Path):
    """Write tables/slai_rumble.json into the S.L.A.I. asset root."""
    with SlaiDisc(image) as disc:
        program = ProgramData(disc)
        hits = disc.find(NAMES_FILE)
        if not hits:
            raise ValueError("Missing " + NAMES_FILE)
        names = read_msg(disc.read(hits[0]))
    table = rumble_table(program, names)
    save(mods / TABLE_FILE, table)
    return {"rumble_regulations": len(table["regulations"]),
            "rumble_matches": len(table["matches"]),
            "rumble_city_days": len(table["rota"]["city_days"])}
