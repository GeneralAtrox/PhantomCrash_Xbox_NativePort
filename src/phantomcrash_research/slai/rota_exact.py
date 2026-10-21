"""An exact S.L.A.I. rota (rota.py) as a mixed-integer programme.

Each (port day, city, source day) is a choice; a day takes three, a city at
most one source a day and never two days running, and at each stage a
regulation at most once a day. A match or ladder record counts once some
choice offers it. Maximising the count (records weighted as in rota.py)
with SciPy's HiGHS finds the full coverage the greedy search misses.
"""

from collections import defaultdict

from .rota import CITIES_PER_DAY, FIRST_STAGE, RECORD_WEIGHT, Lineups, source_days

TIME_LIMIT = 300.0  # seconds; HiGHS returns its best rota so far


def solve_rota(table, port_days, window, time_limit=TIME_LIMIT):
    """The rota, or None without SciPy or when no rota is found in time."""
    try:
        import numpy as np
        from scipy.optimize import Bounds, LinearConstraint, milp
        from scipy.sparse import coo_matrix
    except ImportError:
        return None
    lineups = Lineups(table)
    choices = [(day, city, source) for day in range(port_days)
               for city in range(lineups.cities)
               for source in sorted(source_days(day, port_days, lineups.slai_days, window))]
    offers = defaultdict(list)
    for i, (_, city, source) in enumerate(choices):
        matches, records = lineups.items(city, source)
        for item in matches:
            offers["match", *item].append(i)
        for item in records:
            offers["record", *item].append(i)
    items = sorted(offers)
    weights = [RECORD_WEIGHT if kind == "record" else 1 for kind, _, _ in items]
    columns = len(choices) + len(items)
    rows, cols, values, lower, upper = [], [], [], [], []

    def constrain(coefficients, low, high):
        row = len(lower)
        for column, value in coefficients.items():
            rows.append(row)
            cols.append(column)
            values.append(value)
        lower.append(low)
        upper.append(high)

    by_day = defaultdict(list)
    for i, (day, city, source) in enumerate(choices):
        by_day[day].append(i)
    for day in range(port_days):
        constrain({i: 1 for i in by_day[day]}, CITIES_PER_DAY, CITIES_PER_DAY)
        following = (day + 1) % port_days
        for city in range(lineups.cities):
            both = {i: 1 for i in by_day[day] + by_day[following] if choices[i][1] == city}
            constrain(both, 0, 1)
        for stage in lineups.stages:
            ruled = defaultdict(dict)
            for i in by_day[day]:
                _, city, source = choices[i]
                for match in lineups.cell(city, source)[stage - FIRST_STAGE]:
                    ruled[lineups.regulation[match]][i] = 1
            for coefficients in ruled.values():
                if len(coefficients) > 1:
                    constrain(coefficients, 0, 1)
    for n, item in enumerate(items):
        coefficients = {len(choices) + n: 1}
        coefficients.update({i: -1 for i in offers[item]})
        constrain(coefficients, -np.inf, 0)
    matrix = coo_matrix((values, (rows, cols)), shape=(len(lower), columns)).tocsr()
    objective = np.zeros(columns)
    objective[len(choices):] = [-w for w in weights]
    result = milp(objective, constraints=LinearConstraint(matrix, lower, upper),
                  integrality=np.ones(columns), bounds=Bounds(0, 1),
                  options={"time_limit": time_limit, "disp": False,
                           "mip_rel_gap": 0.0})
    if result.x is None:
        return None
    rota = [[] for _ in range(port_days)]
    for i, (day, city, source) in enumerate(choices):
        if result.x[i] > 0.5:
            rota[day].append((city, source))
    return rota

