"""S.L.A.I. talk scenes: ANB animation banks, PSMT4 pictures, scripts and tables."""

from pathlib import Path
import struct
import unittest

from phantomcrash_research.slai import pig
from phantomcrash_research.slai.anb import read_anb, write_anb
from phantomcrash_research.slai.archive import SlaiDisc
from phantomcrash_research.slai.campaign import EVT_OPCODE_NAMES, read_evt
from phantomcrash_research.slai.program import ProgramData
from phantomcrash_research.slai.talk_font import METRIC_NARROW, METRIC_TABLE, METRIC_WIDE, metric_entry
from phantomcrash_research.slai.talk_tables import evt_json, json_evt

DISC = Path('C:/Projects/SLAI/SLAI.iso')


def _cell(texture, x, blend='alpha'):
    names = ('u0', 'v0', 'u1', 'v1', 'x', 'y', 'half_width', 'half_height', 'scale_x',
             'scale_y', 'rotation', 'origin_x', 'origin_y', 'scale2_x', 'scale2_y', 'rotation2')
    cell = {name: 0 for name in names}
    cell.update(texture=texture, x=x, u1=64, v1=32, half_width=32, half_height=16.5,
                scale_x=1, scale_y=1, scale2_x=1, scale2_y=1, blend=blend, rgba=[255, 128, 0, 255])
    return cell


def _pig(width, height, fmt, pixels, palette, flags=0xE):
    header = bytearray(0x50)
    header[:4] = b'P2IG'
    struct.pack_into('<H', header, 0xC, flags)
    struct.pack_into('<3H', header, 0x20, width.bit_length() - 1, height.bit_length() - 1, fmt)
    struct.pack_into('<4I', header, 0x40, 0x50, len(palette), 0x50 + len(palette), len(pixels))
    return bytes(header) + palette + pixels


class TalkFormatTests(unittest.TestCase):
    def test_anb_round_trip(self):
        bank = {'version': 0x78, 'textures': 2,
                'cells': [_cell(0, 0), _cell(1, -12.5, 'additive')],
                'frames': [[0, 1], None, [1]],
                'anims': [[{'op': 'frame', 'a': 0, 'b': 4}, {'op': 'tween', 'a': 1, 'b': 8},
                           {'op': 'frame', 'a': 2, 'b': 4}, {'op': 'loop'}, {'op': 'end'}],
                          [{'op': 'jump', 'anim': 0}, {'op': 'end'}]]}
        data = write_anb(bank)
        self.assertEqual(read_anb(data), bank)
        self.assertEqual(write_anb(read_anb(data)), data)
        with self.assertRaises(ValueError):
            read_anb(b'\0' * 48)

    def test_psmt4_unswizzle_is_a_permutation(self):
        for width, height in ((128, 64), (256, 64)):
            size = width * height // 2
            seen = set()
            for nibble in range(0, width * height, 97):
                packed = bytearray(size)
                packed[nibble // 2] = 1 << (nibble % 2 * 4)
                out = pig._unswizzle4(bytes(packed), width, height)
                hits = [i for i, v in enumerate(out) if v]
                self.assertEqual(len(hits), 1, (width, height, nibble))
                seen.add(hits[0])
            self.assertEqual(len(seen), len(range(0, width * height, 97)))

    def test_unswizzled_flag_reads_pixels_linearly(self):
        palette = b''.join(bytes((i * 16, 0, 0, 0x80)) for i in range(16))
        pixels = bytes((0x21, 0x43)) * (16 * 16 // 4)
        image = pig.read_pig(_pig(16, 16, pig.PSMT4, pixels, palette, flags=0x6))
        self.assertEqual([image.getpixel((x, 0))[0] for x in range(4)], [16, 32, 48, 64])
        self.assertEqual(image.getpixel((0, 0))[3], 255)

    def test_evt_json_round_trip(self):
        code = struct.pack('<14H', 0x14, 30, 0x0B, 0, 2, 0xFFD8, 50, 0x21, 0, 0x20, 0x35, 1, 0, 0)
        strings = [struct.pack('<3H', 0x28, 0x49, 0xFFFF), struct.pack('<2H', 0x21, 0xFFFF)]
        start = 4 + 4 * 3
        offsets = [start, start + len(code), start + len(code) + len(strings[0])]
        data = struct.pack('<4I', 2, *offsets) + code + b''.join(strings)
        script = evt_json(data)
        self.assertEqual(script['code'][1], ['open_window', 0, 2, -40, 50])
        self.assertEqual(script['strings'], ['Hi', 'A'])
        rebuilt = read_evt(json_evt(script))
        self.assertEqual([c[1:] for c in rebuilt['code']], [c[1:] for c in read_evt(data)['code']])
        self.assertEqual(rebuilt['strings'], script['strings'])
        with self.assertRaises(ValueError):
            json_evt({'code': [['say']], 'strings': []})

    def test_opcode_names_are_unique(self):
        self.assertEqual(len(set(EVT_OPCODE_NAMES.values())), 74)
        self.assertEqual(EVT_OPCODE_NAMES[0x21], 'say')

    def test_glyph_metric_ranges(self):
        self.assertEqual(metric_entry(0x41), METRIC_TABLE + 0x82)
        self.assertEqual(metric_entry(0x132), METRIC_NARROW)
        self.assertEqual(metric_entry(0x2ED), METRIC_NARROW)
        self.assertEqual(metric_entry(0x300), METRIC_WIDE)


@unittest.skipUnless(DISC.exists(), 'Local S.L.A.I. disc unavailable')
class TalkDiscTests(unittest.TestCase):
    @classmethod
    def setUpClass(cls):
        from phantomcrash_research.slai.talk_tables import package_directory, talk_table
        cls.disc = SlaiDisc(DISC)
        program = ProgramData(cls.disc)
        cls.folders = package_directory(program)
        cls.table = talk_table(program)

    @classmethod
    def tearDownClass(cls):
        cls.disc.close()

    def _file(self, folder, name):
        files = self.disc.packages()[self.folders[folder]['offset']]
        return self.disc.read(next(f for f in files if f.name == name))

    def test_opening_events(self):
        events = {e['name']: e for e in self.table['events']}
        self.assertEqual(events['main01t']['kind'], 'town')
        self.assertEqual(events['main01t']['flags'], ['town_overlay'])
        self.assertEqual(events['main02k']['required_flag'], 0)
        self.assertEqual((events['bs01k']['place'], events['bs01k']['sub_place']),
                         ('townmap', 'tutorial'))
        self.assertEqual(events['bs05k']['condition'], 'tutorial_not_cleared')
        self.assertEqual(events['main05k']['completion_flag'], 5)

    def test_story_rules(self):
        events = {e['name']: e for e in self.table['events']}
        self.assertEqual(events['bs03k']['condition'], 'tutorial_cleared_first_day')
        self.assertEqual(events['rec_data03k']['condition'], 'defeated_over_999')
        self.assertEqual(events['main32k']['condition'], 'junk_module_shop')
        self.assertEqual(events['main67k']['condition'], 'last_battle_lost')
        self.assertEqual(self.table['overnight_flags'][0], [27, 28])
        self.assertEqual(self.table['overnight_flags'][-1], [123, 124])
        self.assertEqual(len(self.table['overnight_flags']), 10)
        paths = self.table['partner_paths']
        self.assertEqual([len(p['steps']) for p in paths], [25, 10, 7])
        self.assertEqual(paths[2], {'horizontal': True, 'steps': [1, 3, 1, -1, -3, -1, 0]})
        points = sum(p['points'] for p in self.table['completion_score']['parts'])
        self.assertEqual(points, 10000)

    def test_characters_news_and_folders(self):
        self.assertEqual(self.table['characters']['2'],
                         {'folder': 'ra000', 'name': 'Embryo', 'chip_icon': 60,
                          'expressions': 9, 'pictures': 2})
        self.assertEqual(self.table['characters']['58']['name'], 'JJ')
        self.assertEqual(self.table['news_pictures'][32], 'talk/news/1/news180.png')
        self.assertEqual(self.folders['TALK/EVENT/UK']['offset'], 0x1C31000)
        self.assertEqual(self.table['opcodes'][0x2F]['waits_for'], ['portrait'])
        self.assertEqual(self.table['panel_sprites'][3], [166, 217])
        self.assertEqual(self.table['login_progress'][-1], [240, 100])
        self.assertEqual(self.table['voice_meter'][2][2], {'low': 0.4, 'high': 1.0,
                                                           'phase': 2.9671, 'period': 10})
        self.assertEqual(self.table['window_colours'][2], '80804b18')
        self.assertEqual(self.table['knocker_textures'][0x13], 'talk/de000/de0000.png')

    def test_talk_window_art_decodes(self):
        bank = read_anb(self._file('TALK/COMMON', 'TAL01.ANB'))
        self.assertEqual((bank['textures'], len(bank['anims'])), (5, 46))
        font = pig.read_pig(self._file('FONT', 'FONT01.PIG'))
        self.assertEqual(font.size, (512, 512))
        cell = font.crop((33 * 16 % 512, 33 // 32 * 18, 33 * 16 % 512 + 16, 33 // 32 * 18 + 18))
        opaque = sum(1 for x in range(16) for y in range(18) if cell.getpixel((x, y))[3])
        self.assertGreater(opaque, 20)  # "A" is glyph 0x21
        portrait = pig.read_pig(self._file('TALK/RA000', 'RA0000.PIG'))
        self.assertEqual(portrait.getpixel((128, 128))[3], 255)


if __name__ == '__main__':
    unittest.main()
