import math
import struct
import unittest

from phantomcrash_research.slai.steering_inputs import GUARDS, decode_steering_inputs


class Program:
    def __init__(self, **changes):
        self.words = {**GUARDS, **changes}

    def read(self, address, size):
        return struct.pack('<I', self.words[address])


class SteeringInputsTests(unittest.TestCase):
    def test_shared_stick_filter_and_pad_scale_are_recovered(self):
        inputs = decode_steering_inputs(Program())
        stick = inputs['shared_stick']
        self.assertAlmostEqual(stick['turn_dead_angle'], math.pi / 6, places=6)
        self.assertAlmostEqual(stick['turn_dead_far'], 5 * math.pi / 6, places=6)
        self.assertAlmostEqual(stick['aim_dead_low'], math.radians(70), places=6)
        self.assertAlmostEqual(stick['aim_dead_high'], math.radians(110), places=6)
        self.assertEqual(stick['rescale_offset'], 55 / 128)
        self.assertEqual(stick['rescale_range'], 73 / 128)
        self.assertEqual(inputs['pad'], {'digital_scale': 127, 'stick_dead_zone': 55,
                                         'turn_scale': 127.0})
        self.assertTrue(inputs['release'].startswith('none'))

    def test_turn_speed_load_rule_playback_and_cadence_are_recovered(self):
        inputs = decode_steering_inputs(Program())
        load = inputs['source_load']
        self.assertEqual((load['level_max'], load['overload_band'], load['graded_bands'],
                          load['overload_power']), (99, 35.0, 3, 10.0))
        self.assertAlmostEqual(load['level_trim'], .1, places=6)
        self.assertAlmostEqual(inputs['turn_speed']['min'], .1, places=6)
        self.assertEqual(inputs['turn_speed']['max'], 90.0)
        self.assertAlmostEqual(inputs['playback']['step'], math.pi / 40, places=6)
        self.assertAlmostEqual(inputs['playback']['min'], .9, places=6)
        self.assertAlmostEqual(inputs['playback']['max'], 1.1, places=6)
        self.assertEqual(inputs['cadence'], {'source_tick_rate': 60, 'source_frame_ticks': 2})

    def test_every_producer_is_guarded(self):
        for address in (0x4debf8, 0x4dec3c, 0x4de760, 0x4dd8dc, 0x2569cc, 0x2569d8,
                        0x4ebc64, 0x4ebcd0, 0x1cbc84, 0x4e56fc):
            with self.assertRaises(ValueError):
                decode_steering_inputs(_changed(address))


def _changed(address):
    program = Program()
    program.words[address] = 0
    return program


if __name__ == '__main__':
    unittest.main()
