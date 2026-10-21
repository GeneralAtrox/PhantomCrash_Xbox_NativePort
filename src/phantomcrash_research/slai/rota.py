"""S.L.A.I. Rumble's rota on Phantom Crash's calendar (data/world/slai_rota.json).

S.L.A.I. runs its rumbles on a 100-day calendar in seven cities; the port
squeezes them into Phantom Crash's 60-day one (docs/SLAI_RUMBLE_PLAN.md,
"Days"). Each port day opens three cities, none of which was open the day
before, and each open city shows its line-up from one S.L.A.I. day near the
port day (within `window` days of the S.L.A.I. days the port day maps to).
No regulation repeats among a day's cities at any stage, so every day is a
mix. Within those rules the rota covers as many of the ladder's records and
as many distinct matches as it can.

The rota holds no disc content, only city ids and S.L.A.I. day numbers. It
is generated from the extracted tables/slai_rumble.json:

    python -m phantomcrash_research.slai.rota TABLE.json data/world/slai_rota.json
"""

from collections import Counter
from itertools import combinations, product
from pathlib import Path
import json
import math
import random
import sys

ROTA_VERSION = 1
PORT_DAYS = 60
CITIES_PER_DAY = 3
WINDOW = 2
SEED = 1
CLIMB_STEPS = 400_000
# The annealing's starting temperature, in matches: a lost match is often
# taken early on, a lost ladder record (RECORD_WEIGHT) practically never.
HEAT = 1.5
# Stage 0 is the tutorial (match 0 everywhere); the rules and coverage
# count stages 1 to 4.
FIRST_STAGE = 1
# The ladder's records (0x257440, 0x257490): stage 1 needs every ranker
# record cleared, stage 2 every regional one, so the rota must offer them
# at those stages.
LADDER_RECORDS = {1: range(0x30, 0x68), 2: range(0x68, 0x6F)}
RECORD_WEIGHT = 1000


def source_days(day, port_days=PORT_DAYS, slai_days=100, window=WINDOW):
    """The S.L.A.I. days a port day may borrow from, nearest first."""
    first = day * slai_days // port_days
    last = (day + 1) * slai_days // port_days - 1
    days = range(first - window, max(first, last) + window + 1)
    middle = (first + last) / 2
    return sorted({d % slai_days for d in days}, key=lambda d: (abs(d - middle), d))


class Lineups:
    """Each city's matches on each S.L.A.I. day, by stage."""

    def __init__(self, table):
        self.cities = len(table["cities"])
        self.slai_days = table["schedule_days"]
        self.stages = range(FIRST_STAGE, table["stages"])
        rows = table["rota"]["city_days"]
        days = table["rota"]["days"]
        self.regulation = {m["id"]: m["regulation"] for m in table["matches"]}
        self.record = {m["id"]: m["save_record"] for m in table["matches"]}
        self.cells = {}
        for slai_day, row in enumerate(days):
            for city, city_day in enumerate(row["city_days"]):
                stages = rows[city_day]["stages"]
                self.cells[city, slai_day] = tuple(
                    tuple(m for m in stages[s] if m >= 0) for s in self.stages)

    def cell(self, city, slai_day):
        return self.cells[city, slai_day]

    def items(self, city, slai_day):
        """What a cell offers: (stage, match) and the ladder's (stage, record)."""
        matches, records = [], []
        for stage, ids in zip(self.stages, self.cell(city, slai_day)):
            for match in ids:
                matches.append((stage, match))
                if self.record[match] in LADDER_RECORDS.get(stage, ()):
                    records.append((stage, self.record[match]))
        return matches, records

    def mixed(self, entries):
        """No regulation repeats among the entries at any stage."""
        for index in range(len(self.stages)):
            seen = [self.regulation[m] for c, d in entries for m in self.cell(c, d)[index]]
            if len(seen) != len(set(seen)):
                return False
        return True

    def universe(self):
        matches, records = set(), set()
        for city, slai_day in self.cells:
            found, ladder = self.items(city, slai_day)
            matches.update(found)
            records.update(ladder)
        return matches, records


class Coverage:
    """Counts of what the rota offers, for incremental scoring."""

    def __init__(self, lineups):
        self.lineups = lineups
        self.matches = Counter()
        self.records = Counter()

    def add(self, city, slai_day, sign=1):
        matches, records = self.lineups.items(city, slai_day)
        for key in matches:
            self.matches[key] += sign
        for key in records:
            self.records[key] += sign

    def score(self):
        return (RECORD_WEIGHT * sum(1 for n in self.records.values() if n > 0) +
                sum(1 for n in self.matches.values() if n > 0))


def _first_rota(lineups, windows, rng):
    """A rota that keeps the rules, built a day at a time, greedy for coverage."""
    every = range(lineups.cities)
    rota, coverage = [], Coverage(lineups)

    def options(day):
        before = {c for c, _ in rota[-1]} if rota else set()
        after = {c for c, _ in rota[0]} if day == len(windows) - 1 else set()
        allowed = [c for c in every if c not in before and c not in after]
        found = []
        for trio in combinations(allowed, CITIES_PER_DAY):
            for sources in product(windows[day], repeat=CITIES_PER_DAY):
                entries = list(zip(trio, sources))
                if lineups.mixed(entries):
                    found.append(entries)
        return found

    def gain(entries):
        before = coverage.score()
        for entry in entries:
            coverage.add(*entry)
        after = coverage.score()
        for entry in entries:
            coverage.add(*entry, sign=-1)
        return after - before + rng.random()

    stack = []
    while len(rota) < len(windows):
        day = len(rota)
        if len(stack) <= day:
            stack.append(sorted(options(day), key=gain))
        if not stack[day]:
            stack.pop()
            if not rota:
                raise ValueError("no rota keeps the rules")
            for entry in rota.pop():
                coverage.add(*entry, sign=-1)
            continue
        entries = stack[day].pop()
        rota.append(entries)
        for entry in entries:
            coverage.add(*entry)
    return rota, coverage


def _climb(lineups, windows, rota, coverage, rng, steps):
    """Anneals single entries (a new source day, or a new city), cooling
    from HEAT to nothing, and keeps the best rota met."""
    days = len(rota)
    every = range(lineups.cities)
    score = coverage.score()
    best, best_score = [list(e) for e in rota], score
    for step in range(steps):
        heat = HEAT * (1.0 - step / steps)
        day = rng.randrange(days)
        slot = rng.randrange(CITIES_PER_DAY)
        city, source = rota[day][slot]
        if rng.random() < 0.5:
            near = {c for d in (day - 1, day, (day + 1) % days) for c, _ in rota[d]}
            free = [c for c in every if c not in near]
            if not free:
                continue
            city = rng.choice(free)
        source = rng.choice(windows[day])
        entries = list(rota[day])
        old = entries[slot]
        entries[slot] = (city, source)
        if (city, source) == old or not lineups.mixed(entries):
            continue
        coverage.add(*old, sign=-1)
        coverage.add(city, source)
        new = coverage.score()
        if new >= score or (heat > 0 and rng.random() < math.exp((new - score) / heat)):
            rota[day] = entries
            score = new
            if score > best_score:
                best, best_score = [list(e) for e in rota], score
        else:
            coverage.add(city, source, sign=-1)
            coverage.add(*old)
    coverage = Coverage(lineups)
    for entries in best:
        for entry in entries:
            coverage.add(*entry)
    return best, coverage


def _best_day(lineups, windows, rota, coverage, day):
    """The best entries for a day its neighbours allow, with the day's own
    entries taken out of `coverage`: (entries, gain), or (None, None)."""
    days = len(rota)
    near = {c for d in (day - 1, (day + 1) % days) for c, _ in rota[d]}
    free = [c for c in range(lineups.cities) if c not in near]
    base = coverage.score()
    best, best_gain = None, None
    for trio in combinations(free, CITIES_PER_DAY):
        for sources in product(windows[day], repeat=CITIES_PER_DAY):
            entries = list(zip(trio, sources))
            if not lineups.mixed(entries):
                continue
            for entry in entries:
                coverage.add(*entry)
            gain = coverage.score() - base
            for entry in entries:
                coverage.add(*entry, sign=-1)
            if best_gain is None or gain > best_gain:
                best, best_gain = entries, gain
    return best, best_gain


def _set_day(rota, coverage, day, entries):
    for entry in rota[day]:
        coverage.add(*entry, sign=-1)
    rota[day] = list(entries)
    for entry in entries:
        coverage.add(*entry)


def _polish(lineups, windows, rota, coverage, rng):
    """Re-chooses whole days, each the best of every day its neighbours
    allow, until no day improves."""
    days = len(rota)
    improved = True
    while improved:
        improved = False
        for day in rng.sample(range(days), days):
            score = coverage.score()
            kept = rota[day]
            _set_day(rota, coverage, day, [])
            best, _ = _best_day(lineups, windows, rota, coverage, day)
            _set_day(rota, coverage, day, best if best is not None else kept)
            if coverage.score() > score:
                improved = True
            else:
                _set_day(rota, coverage, day, kept)
    return rota, coverage


def build_rota(table, port_days=PORT_DAYS, window=WINDOW, seed=SEED, steps=CLIMB_STEPS,
               exact=True):
    """The rota: per port day, CITIES_PER_DAY (city, S.L.A.I. day) pairs.
    Solved exactly with SciPy when it is installed (rota_exact.py), else
    searched: greedy, annealed, then polished a day at a time, which may
    miss a few matches."""
    if exact:
        from .rota_exact import solve_rota
        rota = solve_rota(table, port_days, window)
        if rota is not None:
            return _ordered(table, rota)
    lineups = Lineups(table)
    windows = [source_days(d, port_days, lineups.slai_days, window) for d in range(port_days)]
    rng = random.Random(seed)
    rota, coverage = _first_rota(lineups, windows, rng)
    rota, coverage = _climb(lineups, windows, rota, coverage, rng, steps)
    rota, coverage = _polish(lineups, windows, rota, coverage, rng)
    return _ordered(table, rota)


def _ordered(table, rota):
    """Each day's cities in the schedule's display order."""
    order = table["display_order"]
    return [sorted(entries, key=lambda entry: order.index(entry[0])) for entries in rota]


def rota_errors(table, rota, window=WINDOW):
    """Where a rota breaks the rules; empty when it keeps them."""
    lineups = Lineups(table)
    errors = []
    days = len(rota)
    for day, entries in enumerate(rota):
        cities = [c for c, _ in entries]
        allowed = source_days(day, days, lineups.slai_days, window)
        if len(entries) != CITIES_PER_DAY or len(set(cities)) != len(cities):
            errors.append(f"day {day}: needs {CITIES_PER_DAY} different cities")
        if any(not 0 <= c < lineups.cities for c in cities):
            errors.append(f"day {day}: unknown city")
            continue
        if set(cities) & {c for c, _ in rota[(day + 1) % days]}:
            errors.append(f"day {day}: a city opens again the next day")
        if any(s not in allowed for _, s in entries):
            errors.append(f"day {day}: a source day is outside {sorted(allowed)}")
            continue
        if not lineups.mixed(entries):
            errors.append(f"day {day}: a regulation repeats")
    return errors


def rota_coverage(table, rota):
    """Per stage: (distinct matches offered, distinct matches in S.L.A.I.),
    and the ladder's records offered and needed."""
    lineups = Lineups(table)
    coverage = Coverage(lineups)
    for entries in rota:
        for entry in entries:
            coverage.add(*entry)
    matches, records = lineups.universe()
    result = {}
    for stage in lineups.stages:
        result[stage] = {
            "matches": [sum(1 for s, _ in coverage.matches if s == stage),
                        sum(1 for s, _ in matches if s == stage)],
            "records": [sum(1 for s, _ in coverage.records if s == stage),
                        sum(1 for s, _ in records if s == stage)],
        }
    return result


def rota_document(table, rota, window=WINDOW):
    coverage = rota_coverage(table, rota)
    lines = ", ".join(f"stage {s} {c['matches'][0]}/{c['matches'][1]}"
                      for s, c in coverage.items())
    names = ", ".join(f"{c['id']} {c['name']}" for c in table["cities"])
    return {
        "notes": [
            "S.L.A.I. Rumble's rota on Phantom Crash's 60-day calendar "
            "(docs/SLAI_RUMBLE_PLAN.md, Days): for each port day, the cities open that "
            "day and the S.L.A.I. day (0-based) whose line-up each shows. The rest are "
            "CLOSED. Generated by python -m phantomcrash_research.slai.rota; hand edits "
            "and mods may replace it.",
            "Rules the generator keeps: three cities a day, none open two days running "
            "(the calendar wraps), no regulation repeating among a day's cities at any "
            f"stage, and each source day within {window} days of the S.L.A.I. days the "
            "port day maps to (port day p covers p*100/60 up to (p+1)*100/60).",
            f"Cities: {names}.",
            f"Distinct matches offered when generated: {lines}; every stage 1 ranker "
            "and stage 2 regional record is offered at its stage.",
        ],
        "version": ROTA_VERSION,
        "window": window,
        "days": [[{"city": c, "slai_day": s} for c, s in entries] for entries in rota],
    }


def read_rota(document):
    return [[(e["city"], e["slai_day"]) for e in entries] for entries in document["days"]]


def main(argv):
    if len(argv) != 2:
        print(__doc__.strip().splitlines()[-1].strip())
        return 2
    table = json.loads(Path(argv[0]).read_text(encoding="utf-8"))
    rota = build_rota(table)
    errors = rota_errors(table, rota)
    if errors:
        print("\n".join(errors))
        return 1
    document = rota_document(table, rota)
    Path(argv[1]).write_text(_dump(document), encoding="utf-8", newline="\n")
    for stage, found in rota_coverage(table, rota).items():
        print(f"stage {stage}: matches {found['matches'][0]}/{found['matches'][1]}, "
              f"ladder records {found['records'][0]}/{found['records'][1]}")
    return 0


def _dump(document):
    """One port day a line, the notes first."""
    text = json.dumps({k: v for k, v in document.items() if k != "days"}, indent=1,
                      ensure_ascii=False)
    days = ",\n".join("  " + json.dumps(entries) for entries in document["days"])
    return text[:-2] + ',\n "days": [\n' + days + "\n ]\n}\n"


if __name__ == "__main__":
    sys.exit(main(sys.argv[1:]))
