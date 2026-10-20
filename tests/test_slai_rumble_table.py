"""The exported tables/slai_rumble.json against campaign.py's decoders."""

from pathlib import Path
import json
import struct
import tempfile
import unittest

from phantomcrash_research.slai.archive import SlaiDisc
from phantomcrash_research.slai.campaign import (
    CITIES, NO_MATCH, matches, read_msg, regulations, schedule, scheduled_match,
)
from phantomcrash_research.slai.program import ProgramData
from phantomcrash_research.slai.rumble_table import (
    DISPLAY_ORDER, TABLE_FILE, _f32, _finals, register_rumble, table_match,
)

DISC = Path('C:/Projects/SLAI/SLAI.iso')


def _synthetic():
    """A rota in campaign.py's shape and the same rota in the exported shape."""
    cities = [[(10 + c, NO_MATCH), (20 + c, NO_MATCH), (30, NO_MATCH),
               (NO_MATCH, 40) if c == 0 else ((2, 6, 3, 5, 40, 41, NO_MATCH)[c], 6),
               (50, NO_MATCH)] for c in range(7)]
    fallback = [(70 + c, 80 + c) for c in range(7)]
    rota = {'days': [{'cities': cities}] * 100, 'finals_fallback': fallback}
    table = {'schedule_days': 100, 'finals': _finals(fallback),
             'rota': {'days': [{'city_days': list(range(7))}] * 100,
                      'city_days': [{'stages': [list(s) for s in city]} for city in cities]}}
    return rota, table


class RumbleTableFormatTests(unittest.TestCase):
    def test_weights_keep_the_shortest_float32_text(self):
        value = struct.unpack('<f', struct.pack('<f', 0.05))[0]
        self.assertEqual(_f32(value), 0.05)
        self.assertEqual(_f32(0.0), 0.0)

    def test_finals_data_reproduces_the_rota_lookup(self):
        rota, table = _synthetic()
        for cleared in range(4):
            for city in range(7):
                for stage in range(5):
                    for night in (False, True):
                        self.assertEqual(
                            table_match(table, 3, city, stage, night, cleared),
                            scheduled_match(rota, 3, city, stage, night, cleared),
                            (cleared, city, stage, night))


@unittest.skipUnless(DISC.exists(), 'Local S.L.A.I. disc unavailable')
class RumbleTableDiscTests(unittest.TestCase):
    @classmethod
    def setUpClass(cls):
        with tempfile.TemporaryDirectory() as root:
            cls.summary = register_rumble(DISC, Path(root))
            cls.table = json.loads((Path(root) / TABLE_FILE).read_text(encoding='utf-8'))
        with SlaiDisc(DISC) as disc:
            cls.program = ProgramData(disc)
            cls.names = read_msg(disc.read(disc.find('UK_RUMBLETYPENAME.MSG')[0]))
        cls.rota = schedule(cls.program)

    def test_counts_cities_and_display_order(self):
        self.assertEqual(self.summary, {'rumble_regulations': 32, 'rumble_matches': 157,
                                        'rumble_city_days': 700})
        self.assertEqual([c['name'] for c in self.table['cities']], list(CITIES))
        self.assertEqual(self.table['display_order'], list(DISPLAY_ORDER))
        self.assertEqual(len(self.table['rota']['days']), 100)
        self.assertEqual(len(self.table['rota']['city_days']), 700)

    def test_regulations_carry_names_and_decoded_rules(self):
        exported = self.table['regulations']
        self.assertEqual([r['name'] for r in exported], self.names)
        for row, rule in zip(exported, regulations(self.program)):
            for key in ('id', 'entry_requirement', 'maker', 'leg_classes', 'special',
                        'param_20', 'prize'):
                self.assertEqual(row[key], rule[key], (row['id'], key))
        self.assertEqual(exported[22]['name'], 'CLASS FREE (Rumbling)')
        self.assertEqual((exported[6]['maker_name'], exported[6]['prize']), ('Proton', 3500))

    def test_matches_keep_raw_fields_and_float32_weights(self):
        for row, match in zip(self.table['matches'], matches(self.program)):
            self.assertEqual((row['robots_min'], row['robots_max']), match['robots'])
            for key in ('regulation', 'night', 'level', 'save_record', 'save_record_b'):
                self.assertEqual(row[key], match[key], (row['id'], key))
            self.assertEqual(struct.pack('<10f', *row['weights']),
                             struct.pack('<10f', *match['weights']), row['id'])
        self.assertEqual([self.table['matches'][i]['save_record'] for i in (2, 3, 4)],
                         [26, 27, 28])

    def test_rota_and_finals_match_scheduled_match_for_every_slot(self):
        self.assertEqual(self.table['finals']['fallback'][0], [78, 84])
        for day in range(100):
            patterns = self.table['rota']['days'][day]['city_days']
            self.assertEqual(patterns, [c * 100 + day for c in range(7)])
            for city in range(7):
                for stage in range(5):
                    for night in (False, True):
                        for cleared in range(4) if stage == 3 else (0,):
                            self.assertEqual(
                                table_match(self.table, day, city, stage, night, cleared),
                                scheduled_match(self.rota, day, city, stage, night, cleared),
                                (day, city, stage, night, cleared))

    def test_shop_lineups_and_provenance(self):
        days = self.table['rota']['days']
        self.assertEqual([d['shop_lineups'] for d in days],
                         [d['header'] for d in self.rota['days']])
        self.assertTrue(all(len(r['shop_lineups']) == 10 for r in self.table['rota']['city_days']))
        provenance = self.table['provenance']
        self.assertEqual(provenance['tables']['city_days'], '0x422ce0')
        self.assertEqual(sorted(provenance['guards']), ['0x2572c0', '0x257350'])


if __name__ == '__main__':
    unittest.main()
