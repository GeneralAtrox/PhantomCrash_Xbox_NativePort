"""S.L.A.I. campaign text, event scripts and Club Wired rumble tables."""

from pathlib import Path
import struct
import unittest

from phantomcrash_research.slai.archive import SlaiDisc
from phantomcrash_research.slai.campaign import (
    CITIES, NO_MATCH, disassemble_evt, evt_opcode_argc, glyph_text, matches, next_stage,
    read_evt, read_msg, regulations, schedule, scheduled_match, story_events,
    text_glyphs, write_msg,
)
from phantomcrash_research.slai.program import ProgramData

DISC = Path('C:/Projects/SLAI/SLAI.iso')
ENGLISH_EVT_PACKAGE = 0x1C31000


class CampaignFormatTests(unittest.TestCase):
    def test_glyphs_are_ascii_minus_0x20_with_named_controls(self):
        units = [ord(c) - 0x20 for c in 'Hi '] + [0xFFFA, 0xFFFE, 0xFF93, 0x81, 0xFF9C]
        text = glyph_text(units)
        self.assertEqual(text, 'Hi {player}\n{style:2}{0081}{/style}')
        self.assertEqual(text_glyphs(text), units)
        self.assertEqual(glyph_text(text_glyphs('{007b}')), '{007b}')
        with self.assertRaises(ValueError):
            text_glyphs('caf\xe9')

    def test_msg_round_trip(self):
        strings = ['Dummy', 'CLASS D (Rumbling)', 'Day {player}\nend', '']
        data = write_msg(strings)
        self.assertEqual(struct.unpack_from('<2I', data), (4, 20))
        self.assertEqual(data[20:22], b'$\x00')  # 'D' - 0x20
        self.assertEqual(read_msg(data), strings)
        with self.assertRaises(ValueError):
            read_msg(data[:-2])

    def test_evt_bytecode_stops_at_end_and_resolves_strings(self):
        code = struct.pack('<12H', 0x14, 30, 0x21, 0, 0x20, 0x35, 1, 0, 0x21, 1, 0x3D, 0)
        strings = [struct.pack('<3H', 0x28, 0x49, 0xFFFF), struct.pack('<2H', 0x21, 0xFFFF)]
        start = 4 + 4 * 3
        offsets = [start, start + len(code), start + len(code) + len(strings[0])]
        data = struct.pack('<4I', 2, *offsets) + code + b''.join(strings)
        script = read_evt(data)
        self.assertEqual([op for _, op, _ in script['code']], [0x14, 0x21, 0x20, 0x35, 0x21, 0x3D, 0])
        self.assertEqual(script['code_end'], offsets[1])
        self.assertEqual(script['strings'], ['Hi', 'A'])
        self.assertIn('say             0  ; Hi', disassemble_evt(script)[1])
        broken = struct.pack('<4I', 2, *offsets) + code[:-2] + b'\x00\x01' + b''.join(strings)
        with self.assertRaises(ValueError):
            read_evt(broken)

    def test_scheduled_match_injects_finals_by_cleared_count(self):
        cities = [[(10 + c, NO_MATCH), (20 + c, NO_MATCH), (30, NO_MATCH),
                   (NO_MATCH, 40) if c == 0 else (2 if c == 4 else 40, NO_MATCH),
                   (50, NO_MATCH)] for c in range(7)]
        rota = {'days': [{'cities': cities}] * 100,
                'finals_fallback': [(70 + c, 80 + c) for c in range(7)]}
        self.assertEqual(scheduled_match(rota, 105, 1, 1, False), 21)
        self.assertEqual(scheduled_match(rota, 0, 0, 3, False, 0), NO_MATCH)
        self.assertEqual(scheduled_match(rota, 0, 0, 3, True, 0), 5)
        self.assertEqual(scheduled_match(rota, 0, 4, 3, False, 0), 74)
        self.assertEqual(scheduled_match(rota, 0, 3, 3, False, 1), 3)
        self.assertEqual(scheduled_match(rota, 0, 2, 3, False, 2), 4)
        self.assertEqual(scheduled_match(rota, 0, 2, 3, True, 2), NO_MATCH)
        self.assertEqual(scheduled_match(rota, 0, 4, 3, False, 2), 0x85)
        self.assertEqual(scheduled_match(rota, 0, 5, 3, True, 2), NO_MATCH)

    def test_stage_advances_on_tutorial_rankers_regionals_and_finals(self):
        self.assertEqual(next_stage(0, 0, 0, 0, True), 1)
        self.assertEqual(next_stage(1, 55, 0, 0, True), 1)
        self.assertEqual(next_stage(1, 56, 0, 0, True), 2)
        self.assertEqual(next_stage(2, 56, 7, 0, True), 3)
        self.assertEqual(next_stage(3, 56, 7, 2, True), 3)
        self.assertEqual(next_stage(3, 56, 7, 3, True), 4)


@unittest.skipUnless(DISC.exists(), 'Local S.L.A.I. disc unavailable')
class CampaignDiscTests(unittest.TestCase):
    @classmethod
    def setUpClass(cls):
        with SlaiDisc(DISC) as disc:
            cls.program = ProgramData(disc)
            cls.names = read_msg(disc.read(disc.find('UK_RUMBLETYPENAME.MSG')[0]))
            cls.scripts = {f.name: disc.read(f) for f in disc.packages()[ENGLISH_EVT_PACKAGE]
                           if f.name.endswith('.EVT')}

    def test_regulations_match_their_english_names(self):
        rules = regulations(self.program)
        self.assertEqual(self.names[22], 'CLASS FREE (Rumbling)')
        self.assertEqual((rules[2]['entry_requirement'], rules[2]['prize']), (5000, 18000))
        self.assertEqual(self.names[6], 'For Proton SVs only')
        self.assertEqual((rules[6]['maker'], rules[6]['prize']), (1, 3500))
        self.assertEqual(rules[13]['leg_classes'], ['caterpillar/hover'])
        self.assertEqual(rules[15]['special'], 'rental only')
        self.assertEqual(rules[22]['maker'], None)

    def test_schedule_gives_each_city_one_rumble_per_day(self):
        rota = schedule(self.program)
        rows = matches(self.program)
        for day in rota['days']:
            for city in day['cities']:
                self.assertEqual(sum(i >= 0 for i in city[1]), 1)
                self.assertTrue(all(i < len(rows) for i in city[1]))
        regional = {c: rows[rota['days'][d]['cities'][c][2][s]]['regulation']
                    for d in range(100) for c in range(len(CITIES)) for s in (0, 1)
                    if rota['days'][d]['cities'][c][2][s] >= 0}
        self.assertEqual(len(regional), 7)
        self.assertEqual(rota['finals_fallback'][0], (78, 84))
        self.assertEqual(rows[78]['regulation'], 22)
        self.assertEqual([rows[i]['save_record'] for i in (2, 3, 4)], [26, 27, 28])

    def test_story_events_and_english_scripts_decode(self):
        events = story_events(self.program)
        self.assertEqual(len(events), 154)
        names = [e['name'] for e in events]
        self.assertIn('main01t', names)
        self.assertIn('sponsor00k', names)
        self.assertEqual(sum(bool(n) for n in names), 146)
        argc = evt_opcode_argc(self.program)
        self.assertEqual(len(argc), 74)
        self.assertEqual(len(self.scripts), 143)
        for name, data in self.scripts.items():
            script = read_evt(data)
            limit = struct.unpack_from('<I', data, 8)[0] if script['strings'] else len(data)
            self.assertLessEqual(script['code_end'], limit, name)
        first = read_evt(self.scripts['MAIN01T.EVT'])
        self.assertTrue(any(op == 0x21 for _, op, _ in first['code']))


if __name__ == '__main__':
    unittest.main()
