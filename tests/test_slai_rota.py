"""S.L.A.I. Rumble's rota on the 60-day calendar (slai/rota.py) and the
committed data/world/slai_rota.json."""

from pathlib import Path
import importlib.util
import json
import unittest

import pytest

from phantomcrash_research.slai.rota import (
    CITIES_PER_DAY, PORT_DAYS, build_rota, read_rota, rota_coverage, rota_document,
    rota_errors, source_days,
)

ROOT = Path(__file__).resolve().parents[1]
TABLE = 'artifacts/slai/assets/tables/slai_rumble.json'
ROTA = ROOT / 'data/world/slai_rota.json'


def _table(regulations=5, matches=20):
    """Seven cities over 100 days, each city-day one stage-1 match whose
    regulation is its id modulo `regulations`."""
    city_days, days = [], []
    for day in range(100):
        row = []
        for city in range(7):
            match = (day * 7 + city * 3) % matches
            row.append(len(city_days))
            city_days.append({'stages': [[0, 0], [match, -1]]})
        days.append({'city_days': row})
    return {
        'cities': [{'id': c, 'name': f'City {c}'} for c in range(7)],
        'display_order': [2, 6, 0, 4, 5, 1, 3],
        'schedule_days': 100,
        'stages': 2,
        'matches': [{'id': m, 'regulation': m % regulations, 'save_record': 200 + m}
                    for m in range(matches)],
        'rota': {'days': days, 'city_days': city_days},
    }


class SourceDayTests(unittest.TestCase):
    def test_a_port_day_borrows_within_two_days_of_its_range(self):
        self.assertEqual(sorted(source_days(0)), [0, 1, 2, 98, 99])
        self.assertEqual(sorted(source_days(1)), [0, 1, 2, 3, 4, 99])
        self.assertEqual(sorted(source_days(59)), [0, 1, 96, 97, 98, 99])

    def test_every_slai_day_is_some_port_days_neighbour(self):
        covered = {d for day in range(PORT_DAYS) for d in source_days(day)}
        self.assertEqual(covered, set(range(100)))


class RotaRuleTests(unittest.TestCase):
    def test_the_search_keeps_the_rules(self):
        table = _table()
        rota = build_rota(table, steps=2000, exact=False)
        self.assertEqual(len(rota), PORT_DAYS)
        self.assertEqual(rota_errors(table, rota), [])

    def test_every_day_lists_its_cities_in_display_order(self):
        table = _table()
        order = table['display_order']
        for entries in build_rota(table, steps=0, exact=False):
            self.assertEqual([c for c, _ in entries],
                             sorted((c for c, _ in entries), key=order.index))

    @unittest.skipUnless(importlib.util.find_spec('scipy'), 'SciPy is not installed')
    def test_the_exact_rota_covers_every_match(self):
        table = _table()
        rota = build_rota(table)
        self.assertEqual(rota_errors(table, rota), [])
        found = rota_coverage(table, rota)[1]['matches']
        self.assertEqual(found[0], found[1])

    def test_the_checks_find_each_broken_rule(self):
        table = _table()
        rota = build_rota(table, steps=0, exact=False)
        again = [list(e) for e in rota]
        again[1][0] = (rota[0][0][0], again[1][0][1])
        self.assertIn('day 0: a city opens again the next day', rota_errors(table, again))
        far = [list(e) for e in rota]
        far[30][0] = (far[30][0][0], 90)
        self.assertTrue(any(e.startswith('day 30: a source day is outside')
                            for e in rota_errors(table, far)))
        short = [list(e) for e in rota]
        short[5] = short[5][:CITIES_PER_DAY - 1]
        self.assertIn('day 5: needs 3 different cities', rota_errors(table, short))
        # Every stage-1 match of city-day (city, day) is (7 day + 3 city) % 20:
        # cities 0 and 5 on day 0 hold matches 0 and 15, both regulation 0.
        clash = [list(e) for e in rota]
        clash[0] = [(0, 0), (5, 0), (rota[0][2][0], 0)]
        self.assertIn('day 0: a regulation repeats', rota_errors(table, clash))

    def test_the_document_round_trips(self):
        table = _table()
        rota = build_rota(table, steps=0, exact=False)
        document = json.loads(json.dumps(rota_document(table, rota)))
        self.assertEqual(read_rota(document), [[tuple(e) for e in d] for d in rota])
        self.assertEqual(document['window'], 2)


@pytest.mark.private_inputs(TABLE)
def test_the_committed_rota_keeps_the_rules_and_covers_every_match():
    table = json.loads((ROOT / TABLE).read_text(encoding='utf-8'))
    rota = read_rota(json.loads(ROTA.read_text(encoding='utf-8')))
    assert rota_errors(table, rota) == []
    coverage = rota_coverage(table, rota)
    for stage, found in coverage.items():
        print(f"stage {stage}: matches {found['matches']}, ladder records {found['records']}")
        assert found['records'][0] == found['records'][1], stage
        assert found['matches'][0] == found['matches'][1], stage


def test_the_committed_rota_has_three_cities_a_day_none_two_days_running():
    rota = read_rota(json.loads(ROTA.read_text(encoding='utf-8')))
    assert len(rota) == PORT_DAYS
    for day, entries in enumerate(rota):
        cities = {c for c, _ in entries}
        assert len(cities) == CITIES_PER_DAY, day
        assert not cities & {c for c, _ in rota[(day + 1) % PORT_DAYS]}, day
        assert all(s in source_days(day) for _, s in entries), day
