"""S.L.A.I.'s shops' stock for the port: tables/slai_shops.json.

S.L.A.I.'s town sells from its own tables, chosen by the day and the city
(docs/SLAI_CAMPAIGN_RESEARCH.md, "Shops"):

- the CHIP shop (Difference EG) a lineup of ten CPUs (0x5A4AA0), from the
  city-day row's +0x28;
- the module shop (The Mechanist) a lineup per category (0x5A0320, +0x2A..
  +0x36) and six SVs (0x5A4840, +0x38) whose bodies are of a brand below 8;
- the junk shop (town spot 0xF, flag 0x43B91C) the same categories from the
  junk tables (0x333B00, 0x333FC0, 0x334840, 0x333F70), its row taken from
  the city-day row of the day in its ten-day cycle (0x51C5A0);
- the five dealers three SVs each (0x333A10), by the day row's +0 and the
  dealer's shop id (0x547950).

The registration converts all of it to the port's records and appends them
to the pack's tables: the CPUs as CHIPs (tables/parts.json, balanced by
chip_balance.json), the SVs as robots and every lineup to tables/garage.json.
slai_shops.json then names, by S.L.A.I. day and city-day row, which of the
appended lineups each shop shows, and keeps where the appended records start
so a rerun replaces them. Run it after the parts and cosmetics registrations,
whose ids it uses.
"""

import json
import statistics
import struct
from pathlib import Path

from .archive import SlaiDisc
from .assets import save
from .campaign import CITIES, CITY_DAY_STRIDE, CITY_DAY_TABLE, DAY_STRIDE, DAY_TABLE, read_msg
from .opponent_export import OpponentCatalogue
from .opponent_table import _robot_record
from .opponents import CPUS, _row
from .weapon_balance import reference, scale_curve, value

TABLE_FILE = "tables/slai_shops.json"
TABLE_VERSION = 1
TOWN = "towndvd.bin"
SOLD_CPUS = range(2, 386)  # 48 families of 8 types; 0-1 E-Chips, the rest enemies'
E_CHIP = 0
PARTNER_TYPES = range(2, 10)
CHIP_PICTURES = (0x3327B4, 48)  # "s_chip/<name>" by family - 1
CHIP_LINEUPS = (0x5A4AA0, 67, 0x28)  # 10 x (cpu, level), town overlay
MODULE_LINEUPS = (0x5A0320, 316, 0x38)  # 14 x (id, signed level), town overlay
SV_LINEUPS = (0x5A4840, 50, 12)  # 6 robots, town overlay
DEALER_LINEUPS = (0x333A10, 40, 6)  # 3 robots
JUNK_ROWS = (0x333B00, 71, 16)  # u16 per category 0..6, +0xE the SV row
JUNK_PARTS = (0x333FC0, 136, 16)  # 4 x (id, level), categories 0..4
JUNK_COSMETICS = (0x334840, 24, 24)  # 12 ids, paints and emblems
JUNK_SVS = (0x333F70, 13, 6)  # 3 robots
JUNK_CYCLE = 0x43A908  # by city: the day in each ten whose row the junk shop shows
PAINTS = (0x41E090, 186, 8)  # f_24D660: (name, colour, price, ...)
DAYS, CITY_DAYS = 100, 700
SHOP_LINEUPS_OFFSET = 0x28  # chip, 7 module categories, SVs, junk
SV_BRAND_LIMIT = 8  # 0x5372D0 offers a body brand below this (f_255240)
# Until the finals stage is reached (+0xA18, set by 0x25A7F0 as the stage
# ladder +0xA21 reaches 4) the junk shop's best SV rows show others (0x51C620).
LOCKED_SV_ROWS = {10: 0, 11: 4, 12: 7}
UNLOCK_STAGE = 4
NONE = 0xFFFF
CATEGORIES = ("body", "leg", "arm", "shoulder", "option", "paint", "emblem")
# The dealers S.L.A.I. adds to the port's three (0x4E12A0's shop ids; the
# message file's profiles). Kojima, Ventuno and American Stars stay Phantom
# Crash's.
DEALERS = {"dealer_omsk": {"shop": 3, "profile": 3}, "dealer_ma_fabrik": {"shop": 4, "profile": 4}}
MAKER_TEXT = "UK_MAKER_SYSMES.MSG"
CPU_NAMES = "UK_CPUNAME.MSG"
CPU_INTROS = "UK_RPCPUINTRO.MSG"
EMBLEM_NONE = 205
# The port's whole-number CHIP curves; the rest keep three decimals.
INTEGER_FIELDS = ("inner_value", "outer_value", "price")


def _read(path):
    return json.loads(Path(path).read_text(encoding="utf-8"))


def _rows(program, table, fmt, overlay=None):
    va, count, stride = table
    out = []
    for i in range(count):
        data = program.read(va + i * stride, stride, overlay) if overlay else program.read(
            va + i * stride, stride)
        out.append(list(struct.unpack_from(fmt, data)))
    return out


def _curve(data, offset, kind):
    fmt = {"short": "<3hbb", "float": "<3fbb", "int": "<3ibb"}[kind]
    base, mid, top, knee, low = struct.unpack_from(fmt, data, offset)
    if kind == "float":
        base, mid, top = (round(v, 4) for v in (base, mid, top))
    return {"base": base, "mid": mid, "top": top, "knee": knee, "low_scale": low}


def _cstring(program, va):
    return program.read(va, 32).split(b"\0")[0].decode("ascii")


class _Converter:
    """S.L.A.I. ids to the port's: parts by model, paints by colour,
    stickers in order, CPUs after the existing CHIPs."""

    def __init__(self, catalogue, garage_base, garage, first_chip):
        self.catalogue = catalogue
        self.port = catalogue.port
        # register_cosmetics appends S.L.A.I.'s paints (by colour, PAINT01..)
        # and stickers (ST000..) after Phantom Crash's.
        self.paint_start = len(garage_base["paints"])
        self.sticker_start = len(garage_base["emblems"])
        self.paints = len(garage["paints"]) - self.paint_start
        self.stickers = len(garage["emblems"]) - self.sticker_start
        self.paint_colours = [row[1] for row in _rows(catalogue.program, PAINTS, "<4h")]
        self.first_chip = first_chip
        self.dropped = {}

    def item(self, category, sid):
        kind = CATEGORIES[category]
        port = None
        if kind in ("body", "leg", "arm", "shoulder"):
            port = self.port.get((kind, sid))
        elif kind == "paint" and 0 <= sid < len(self.paint_colours):
            colour = self.paint_colours[sid]
            port = self.paint_start + colour - 1 if 1 <= colour <= self.paints else None
        elif kind == "emblem" and 0 <= sid < self.stickers:
            port = self.sticker_start + sid
        if port is None:
            self.dropped.setdefault(kind, set()).add(sid)
        return port

    def chip(self, cpu):
        """The port CHIP of a CPU: the E-Chip first, then the sold ones."""
        if cpu in SOLD_CPUS:
            return self.first_chip + 1 + cpu - SOLD_CPUS.start
        return self.first_chip if cpu == E_CHIP else None

    def robot(self, robot_id):
        build = self.catalogue.build(robot_id)
        name = (build["body"]["name"] or "") if build["body"] else ""
        record = _robot_record(self.catalogue, build, name.removesuffix(" Body"))
        # S.L.A.I.'s SVs come without a CPU, its partner moving between them;
        # a port SV needs a CHIP to be bought, so they come with the E-Chip.
        chip = self.chip(build["cpu"]["id"])
        record["parts"]["chip"] = {"id": self.first_chip if chip is None else chip, "level": 0}
        emblem = build["emblem"]
        record["emblem"] = self.sticker_start + emblem if 0 <= emblem < self.stickers else EMBLEM_NONE
        return record


def _chip(data, settings, index, name, picture):
    _, _, weight, kind, family = struct.unpack_from("<5h", data)
    track = struct.unpack_from("<f", data, 0x14)[0]
    (low, high), (to_low, to_high) = settings["track"]["from"], settings["track"]["to"]
    chip = {
        "index": index,
        "name": name,
        "chatter": settings["chatter"].get(str(kind), 0),
        "face": {"image": picture, "rect": [0, 0, 64, 64]} if picture else None,
        "inner_shape": data[0xA],
        "track": int(round(to_low + (min(max(track, low), high) - low) * (to_high - to_low)
                           / (high - low))),
        "outer_shape": struct.unpack_from("<h", data, 0x18)[0],
        "lock_frames": struct.unpack_from("<h", data, 0x22)[0],
        "weight": {"base": float(weight), "mid": 0.0, "top": 0.0, "knee": 50, "low_scale": -1},
        # The partner's script case is the type less one (2..9 play 1..8).
        "slai_type": kind if kind in PARTNER_TYPES else 0,
        "slai": {"record": None, "family": family},
    }
    for field, source in settings["fields"].items():
        chip[field] = _curve(data, source["offset"], source["kind"])
    return chip


def _chips(program, disc, settings, base_parts, first_index):
    """The E-Chip, which the port fits to the SVs S.L.A.I. sells without a
    CPU, then the 384 CPUs its CHIP shop sells."""
    names = read_msg(disc.read(disc.find(CPU_NAMES)[0]))
    va, count = CHIP_PICTURES
    pictures = [_cstring(program, p) for p in struct.unpack("<%dI" % count, program.read(va, 4 * count))]
    chips = []
    for cpu in (E_CHIP, *SOLD_CPUS):
        data = _row(program, CPUS, cpu)
        name, family = struct.unpack_from("<h", data)[0], struct.unpack_from("<h", data, 8)[0]
        picture = "2d/" + pictures[family - 1].lower() + ".png" if 1 <= family <= count else None
        chip = _chip(data, settings, first_index + len(chips),
                     names[name] if 0 <= name < len(names) else "", picture)
        chip["slai"]["record"] = cpu
        chips.append(chip)
    if settings.get("enabled", True):
        _balance(chips[1:], chips[:1], base_parts["chips"], settings)
    return chips


def _balance(sold, others, native, settings):
    """Per field, Phantom Crash's sold CHIPs' median over S.L.A.I.'s at each
    level scales every S.L.A.I. curve (armour_balance.py's rule); a uniform
    field takes the top level's factor throughout. The E-Chip, flat, takes
    the bottom level's."""
    ours = reference(native, settings)
    levels = settings["levels"]
    for field in [*settings["fields"], "weight"]:
        rule = settings["fields"].get(field, settings["weight"])
        factors = []
        for level in levels:
            theirs = statistics.median(value(c[field], level) for c in sold)
            mine = statistics.median(value(r[field], level) for r in ours if field in r)
            factors.append(mine / theirs if theirs > 0 else 1.0)
        if rule.get("uniform"):
            factors = [factors[-1]] * len(factors)
        digits = 0 if field in INTEGER_FIELDS else 3
        for chip in sold:
            chip[field] = scale_curve(chip[field], *factors, digits)
            chip["slai"].setdefault("balance", {})[field] = [round(f, 4) for f in factors]
        for chip in others:
            chip[field] = scale_curve(chip[field], factors[0], factors[0], digits)


class _Stock:
    """Lineups appended to the garage, each converted once."""

    def __init__(self, garage, convert):
        self.garage, self.convert = garage, convert
        self.robots, self.modules, self.svs = {}, {}, {}

    def robot(self, robot_id):
        if robot_id not in self.robots:
            self.robots[robot_id] = len(self.garage["robots"])
            self.garage["robots"].append(self.convert.robot(robot_id))
        return self.robots[robot_id]

    def module_lineup(self, key, category, items):
        if category == CATEGORIES.index("option"):
            return -1  # S.L.A.I.'s options are not the port's modules
        if key not in self.modules:
            lineup = []
            for sid, level in items:
                port = self.convert.item(category, sid)
                if port is not None:
                    lineup.append({"id": port, "level": level})
            self.modules[key] = len(self.garage["module_lineups"])
            self.garage["module_lineups"].append(lineup)
        return self.modules[key]

    def sv_lineup(self, key, robots):
        if key not in self.svs:
            self.svs[key] = len(self.garage["used_sv_lineups"])
            self.garage["used_sv_lineups"].append([self.robot(r) for r in robots])
        return self.svs[key]


def _sellable(catalogue, robot_id):
    build = catalogue.build(robot_id)
    return build["body"] is not None and build["body"]["maker"] < SV_BRAND_LIMIT


def _slai_stock(registration, convert, chips):
    out = {}
    for kind, key in (("body", "body"), ("leg", "legs"), ("arm", "arm"), ("shoulder", "shoulder")):
        ids = sorted(e["part"] for e in registration["parts"]
                     if e["category"] == kind and e["part"] is not None)
        if ids:
            out[key] = [ids[0], ids[-1] + 1]
    out["paint"] = [convert.paint_start, convert.paint_start + convert.paints]
    out["emblem"] = [convert.sticker_start, convert.sticker_start + convert.stickers]
    out["chip"] = [chips[0]["index"], chips[-1]["index"] + 1]
    return out


def shop_tables(disc, assets, pack):
    """The pack's parts and garage with S.L.A.I.'s stock appended, and the
    slai_shops table naming it."""
    registration = _read(pack / "slai/native_parts.json")
    parts, garage = _read(pack / "tables/parts.json"), _read(pack / "tables/garage.json")
    base_parts, base_garage = _read(assets / "tables/parts.json"), _read(assets / "tables/garage.json")
    previous = pack / TABLE_FILE
    if previous.exists():  # a rerun replaces what the last one appended
        bases = _read(previous)["bases"]
        parts["chips"] = parts["chips"][:bases["chips"]]
        for key in ("robots", "module_lineups", "used_sv_lineups", "chip_lineups", "dealer_lineups"):
            garage[key] = garage[key][:bases[key]]
    bases = {"chips": len(parts["chips"]),
             **{k: len(garage[k]) for k in ("robots", "module_lineups", "used_sv_lineups",
                                            "chip_lineups", "dealer_lineups")}}
    settings = _read(Path(__file__).with_name("chip_balance.json"))
    catalogue = OpponentCatalogue(disc, registration)
    program = catalogue.program
    chips = _chips(program, disc, settings, base_parts, bases["chips"])
    parts["chips"].extend(chips)
    convert = _Converter(catalogue, base_garage, garage, bases["chips"])
    stock = _Stock(garage, convert)

    chip_rows = _rows(program, CHIP_LINEUPS, "<20H", TOWN)
    module_rows = _rows(program, MODULE_LINEUPS, "<28h", TOWN)
    sv_rows = _rows(program, SV_LINEUPS, "<6h", TOWN)
    chip_lineups = {}

    def chip_lineup(index):
        if index not in chip_lineups:
            row = chip_rows[index]
            chip_lineups[index] = len(garage["chip_lineups"])
            garage["chip_lineups"].append(
                [{"id": convert.chip(row[i]), "level": row[i + 1]}
                 for i in range(0, 20, 2) if convert.chip(row[i]) is not None])
        return chip_lineups[index]

    city_days = []
    for i in range(CITY_DAYS):
        raw = program.read(CITY_DAY_TABLE + i * CITY_DAY_STRIDE, CITY_DAY_STRIDE)
        lineups = struct.unpack_from("<10H", raw, SHOP_LINEUPS_OFFSET)
        modules = []
        for category in range(7):
            index = lineups[1 + category]
            row = module_rows[index] if index != NONE else []
            items = [(row[j], row[j + 1]) for j in range(0, len(row), 2) if row[j] != -1]
            modules.append(stock.module_lineup(("shop", index), category, items)
                           if index != NONE else -1)
        svs = -1
        if lineups[8] != NONE:
            robots = [r for r in sv_rows[lineups[8]] if r >= 0 and _sellable(catalogue, r)]
            svs = stock.sv_lineup(("shop", lineups[8]), robots)
        city_days.append({"chips": chip_lineup(lineups[0]) if lineups[0] != NONE else -1,
                          "modules": modules, "used_svs": svs,
                          "junk": -1 if lineups[9] == NONE else lineups[9]})

    dealer_rows = _rows(program, DEALER_LINEUPS, "<3h")
    dealer_lineups = {}
    days = []
    for day in range(DAYS):
        raw = program.read(DAY_TABLE + day * DAY_STRIDE, DAY_STRIDE)
        header = struct.unpack_from("<5H7H", raw)
        dealers = []
        for index in header[:5]:
            if index not in dealer_lineups:
                dealer_lineups[index] = len(garage["dealer_lineups"])
                garage["dealer_lineups"].append(
                    [{"robot": stock.robot(r), "picture": -1} for r in dealer_rows[index] if r >= 0])
            dealers.append(dealer_lineups[index])
        days.append({"dealers": dealers, "city_days": list(header[5:])})

    junk_rows = _rows(program, JUNK_ROWS, "<8H")
    junk_parts = _rows(program, JUNK_PARTS, "<8h")
    junk_cosmetics = _rows(program, JUNK_COSMETICS, "<12h")
    junk_svs = _rows(program, JUNK_SVS, "<3h")
    junk = []
    for row in junk_rows:
        modules = []
        for category in range(7):
            index = row[category]
            if category < 5:
                values = junk_parts[index]
                items = [(values[j], values[j + 1]) for j in range(0, 8, 2) if values[j] != -1]
            else:
                items = [(sid, 0) for sid in junk_cosmetics[index] if sid != -1]
            modules.append(stock.module_lineup(("junk", category, index), category, items))
        junk.append({"modules": modules, "sv_row": row[7]})
    sv_lineups = [stock.sv_lineup(("junk", i), [r for r in robots if r >= 0])
                  for i, robots in enumerate(junk_svs)]

    profiles = read_msg(disc.read(disc.find(MAKER_TEXT)[0]))
    table = {
        "version": TABLE_VERSION,
        "cities": list(CITIES),
        "days": days,
        "city_days": city_days,
        "junk": {"cycle": list(program.read(JUNK_CYCLE, len(CITIES))), "rows": junk,
                 "sv_lineups": sv_lineups,
                 "locked_sv_rows": {str(k): v for k, v in LOCKED_SV_ROWS.items()},
                 "unlock_stage": UNLOCK_STAGE},
        "dealers": {name: {"shop": d["shop"], "profile": profiles[d["profile"]].split("\n")}
                    for name, d in DEALERS.items()},
        "slai_stock": _slai_stock(registration, convert, chips),
        "bases": bases,
        "dropped": {k: sorted(v) for k, v in convert.dropped.items()},
        "provenance": {"chips": "0x3df340 CPUs 2..385, chip_balance.json",
                       "chip_lineups": hex(CHIP_LINEUPS[0]), "module_lineups": hex(MODULE_LINEUPS[0]),
                       "sv_lineups": hex(SV_LINEUPS[0]), "dealer_lineups": hex(DEALER_LINEUPS[0]),
                       "junk": "0x333b00, 0x333fc0, 0x334840, 0x333f70, cycle 0x43a908",
                       "day_rows": hex(DAY_TABLE), "city_days": hex(CITY_DAY_TABLE),
                       "profiles": MAKER_TEXT},
    }
    return parts, garage, table


def register_shops(image, assets, pack):
    with SlaiDisc(Path(image)) as disc:
        parts, garage, table = shop_tables(disc, Path(assets), Path(pack))
    save(Path(pack) / "tables/parts.json", parts)
    save(Path(pack) / "tables/garage.json", garage)
    save(Path(pack) / TABLE_FILE, table)
    first, end = table["slai_stock"]["chip"]
    return {"slai_chips": end - first,
            "slai_shop_robots": len(garage["robots"]) - table["bases"]["robots"],
            "slai_dropped": {k: len(v) for k, v in table["dropped"].items()}}
