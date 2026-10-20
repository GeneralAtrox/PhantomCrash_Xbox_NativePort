"""Campaign opponent tables: row layouts, substitution, line-up picking."""

from pathlib import Path
import json
import struct
import unittest

from phantomcrash_research.slai.opponent_rules import (
    LISTS, substitute, pick_lineup,
)
from phantomcrash_research.slai.opponents import (
    parse_opponent, parse_pilot, parse_robot, read_names, resolve_name,
)

DISC = Path('C:/Projects/SLAI/SLAI.iso')
# The port's part registration, written by the S.L.A.I. part importer.
REGISTRATION = next((p for p in (Path(__file__).resolve().parents[1] / 'mods/slai/native_parts.json',
                                 Path('C:/Projects/Xbox/PhantomCrash/mods/slai/native_parts.json'))
                     if p.exists()), Path('mods/slai/native_parts.json'))


def msg(*strings):
    blobs = [b''.join(struct.pack('<H', ord(c) - 0x20) for c in s) + b'\xff\xff'
             for s in strings]
    at, offsets = 4 + 4 * len(strings), []
    for blob in blobs:
        offsets.append(at)
        at += len(blob)
    return struct.pack('<%dI' % (1 + len(strings)), len(strings), *offsets) + b''.join(blobs)


class FixedRandom:
    def __init__(self, *values):
        self.values = list(values)

    def random(self):
        return self.values.pop(0) if self.values else 0.0


def entries(name, count):
    return [{'opponent': 100 * LISTS.index(name) + i, 'record': 1, 'flag': i + 1}
            for i in range(count)]


def found(robots_min, robots_max, **sizes):
    match = {'robots_min': robots_min, 'robots_max': robots_max}
    return {'match': match, 'lists': {n: entries(n, sizes.get(n, 0)) for n in LISTS}}


class SlaiOpponentLayoutTests(unittest.TestCase):
    def test_names_decode_glyphs_and_references(self):
        names = read_names(msg('ENEMY', 'Reese'))
        self.assertEqual(names, ['ENEMY', 'Reese'])
        refs = [(1, 0), (-1, 0)]
        self.assertEqual(resolve_name(refs, names, 0), 'Reese')
        self.assertIsNone(resolve_name(refs, names, 1))
        self.assertIsNone(resolve_name(refs, names, 9))

    def test_opponent_and_pilot_rows(self):
        self.assertEqual(parse_opponent(struct.pack('<4h', 7, 12, 3, 41)),
                         {'pilot': 7, 'robot': 12, 'behaviour': 3, 'level': 41})
        row = bytearray(32)
        struct.pack_into('<3h', row, 0, 500, 501, -1)
        struct.pack_into('<3I2b', row, 8, 10, 200, 3000, 50, -2)
        struct.pack_into('<3h', row, 0x18, 7, 8, 9)
        pilot = parse_pilot(bytes(row))
        self.assertEqual((pilot['name_ref'], pilot['second_name_ref'], pilot['unknown_04']),
                         (500, 501, -1))
        self.assertEqual(pilot['level_curve'],
                         {'base': 10, 'mid': 200, 'top': 3000, 'knee': 50, 'low_scale': -2})
        self.assertEqual((pilot['unknown_18'], pilot['unknown_1a'], pilot['unknown_1c']),
                         (7, 8, 9))

    def test_robot_slots_levels_and_options(self):
        row = bytearray(156)
        struct.pack_into('<12h', row, 0, 10, -99, 82, 99, 4, 5, -1, 0, 95, 1, -1, 0)
        struct.pack_into('<h', row, 0x18, 390)
        struct.pack_into('<h', row, 0x80, 12)
        struct.pack_into('<10h', row, 0x86, 105, 106, *([-1] * 8))
        struct.pack_into('<h', row, 0x9A, 151)
        build = parse_robot(bytes(row))
        self.assertEqual(build['body'], {'id': 10, 'level': -99})
        self.assertEqual(build['legs'], {'id': 82, 'level': 99})
        self.assertEqual(build['arms'], [{'id': 4, 'level': 5}, None])
        self.assertEqual(build['shoulders'], [{'id': 95, 'level': 1}, None])
        self.assertEqual((build['cpu'], build['emblem'], build['unknown_9a']), (390, 12, 151))
        self.assertEqual(build['options'], [105, 106])


class SlaiOpponentRuleTests(unittest.TestCase):
    ROWS = [{'replacement': 1426, 'ids': [50, 52], 'while_set': [], 'while_clear': [0x16]},
            {'replacement': 1427, 'ids': [51, 52], 'while_set': [0x16], 'while_clear': [0x24]}]

    def test_substitution_follows_story_flags(self):
        self.assertEqual(substitute(self.ROWS, 50), 1426)
        self.assertEqual(substitute(self.ROWS, 50, {0x16}), 50)
        self.assertEqual(substitute(self.ROWS, 51), 51)
        self.assertEqual(substitute(self.ROWS, 51, {0x16}), 1427)
        self.assertEqual(substitute(self.ROWS, 51, {0x16, 0x24}), 51)
        self.assertEqual(substitute(self.ROWS, 70), 70)

    def test_first_row_listing_an_id_decides(self):
        self.assertEqual(substitute(self.ROWS, 52, {0x16}), 52)

    def test_leader_first_then_pool_fills_the_last_slots(self):
        lineup = pick_lineup(found(1, 6, leader=1, unbeaten=4, pool=5), FixedRandom())
        self.assertEqual([(e['slot'], e['list']) for e in lineup],
                         [(1, 'leader'), (4, 'pool'), (5, 'pool')])

    def test_main_draws_stop_at_robots_min_and_max(self):
        lineup = pick_lineup(found(3, 6, unbeaten=6, pool=5), FixedRandom())
        self.assertEqual([e['list'] for e in lineup], ['unbeaten'] * 3 + ['pool'] * 2)
        capped = pick_lineup(found(3, 2, unbeaten=6, pool=5), FixedRandom())
        self.assertEqual(len(capped), 2)

    def test_draws_come_from_the_first_three_entries(self):
        lineup = pick_lineup(found(1, 6, unbeaten=6), FixedRandom(0.999))
        self.assertEqual(lineup[0]['opponent'], 2)

    def test_cameo_and_beaten_chances(self):
        cameo = pick_lineup(found(1, 6, cameo=2, unbeaten=2), FixedRandom(0.01, 0.0))
        self.assertEqual(cameo[0]['list'], 'cameo')
        beaten = pick_lineup(found(1, 6, cameo=2, unbeaten=2, beaten=4),
                             FixedRandom(0.5, 0.97, 0.0))
        self.assertEqual(beaten[0]['list'], 'beaten')


@unittest.skipUnless(DISC.exists(), 'Local S.L.A.I. disc unavailable')
class SlaiOpponentDiscTests(unittest.TestCase):
    @classmethod
    def setUpClass(cls):
        from phantomcrash_research.slai.archive import SlaiDisc
        from phantomcrash_research.slai.opponent_export import OpponentCatalogue
        registration = (json.loads(REGISTRATION.read_text(encoding='utf-8'))
                        if REGISTRATION.exists() else None)
        with SlaiDisc(DISC) as disc:
            cls.catalogue = OpponentCatalogue(disc, registration)
            cls.free = cls.catalogue.match_opponents(78)
            cls.regional = cls.catalogue.match_opponents(83)

    def test_free_class_day_rumble_lists(self):
        self.assertEqual(self.free['match']['regulation'], 22)
        self.assertEqual(self.free['status'], 3)
        sizes = {name: len(v) for name, v in self.free['lists'].items()}
        self.assertEqual(sizes, {'unbeaten': 14, 'cameo': 9, 'leader': 0, 'beaten': 0,
                                 'pool': 10})
        first = self.free['lists']['unbeaten'][0]['detail']
        self.assertEqual((first['id'], first['level']), (525, 3))
        self.assertEqual((first['pilot']['name'], first['pilot']['second_name']),
                         ('Onoda-3', 'KenKen6'))
        self.assertEqual(first['body']['id'], 10)

    def test_regional_head_to_head_leader(self):
        self.assertEqual((self.regional['match']['robots_min'],
                          self.regional['match']['robots_max']), (1, 2))
        leader = self.regional['lists']['leader']
        self.assertEqual([e['opponent'] for e in leader], [595])
        detail = leader[0]['detail']
        self.assertEqual((detail['pilot']['name'], detail['level']), ('Masami', 51))
        self.assertEqual((detail['body']['id'], detail['body']['level']), (30, 50))
        self.assertEqual(detail['legs']['id'], 82)

    @unittest.skipUnless(REGISTRATION.exists(), 'Port part registration unavailable')
    def test_variant_records_resolve_to_port_parts(self):
        first = self.free['lists']['unbeaten'][0]['detail']
        self.assertEqual(first['body']['port_part'], 16)
        for entry in self.free['lists']['pool']:
            build = entry['detail']
            for part in [build['body'], build['legs'], *build['arms'], *build['shoulders']]:
                if part is not None:
                    self.assertIsNotNone(part['port_part'])


if __name__ == '__main__':
    unittest.main()
