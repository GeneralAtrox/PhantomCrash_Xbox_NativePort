"""The S.L.A.I. battle camera block comes from the view-screen setup."""

import struct
import unittest

from phantomcrash_research.slai.battle_camera import (
    ASPECT_X_ADDRESS, ASPECT_Y_ADDRESS, SCREEN_DISTANCE_ADDRESS, VIEW_SCREEN_GUARDS, battle_camera)


class Program:
    def __init__(self):
        self.data = {a: struct.pack("<I", w) for a, w in VIEW_SCREEN_GUARDS.items()}
        self.data.update({SCREEN_DISTANCE_ADDRESS: struct.pack("<f", 549.0),
                          ASPECT_X_ADDRESS: struct.pack("<f", 1.0),
                          ASPECT_Y_ADDRESS: struct.pack("<f", 1.16)})

    def read(self, address, size):
        assert len(self.data[address]) == size
        return self.data[address]


class BattleCamera(unittest.TestCase):
    def test_the_vertical_field_of_view_follows_the_screen_distance(self):
        camera = battle_camera(Program())
        # 2 atan(224 / (549 * 1.16)), the 38.76 degrees PCSX2 frames match.
        self.assertAlmostEqual(camera["fov_degrees"], 38.757, places=3)
        self.assertEqual(camera["source"]["screen_distance"], 549.0)
        self.assertEqual(camera["chase_camera"],
                         {"eye_height": 5.0, "behind": 8.6, "height": 0.5, "look_ahead": 20.0})

    def test_a_different_view_screen_setup_is_refused(self):
        for address in VIEW_SCREEN_GUARDS:
            program = Program()
            program.data[address] = bytes(4)
            with self.assertRaises(ValueError):
                battle_camera(program)

    def test_an_invalid_screen_distance_is_refused(self):
        program = Program()
        program.data[SCREEN_DISTANCE_ADDRESS] = struct.pack("<f", 0.0)
        with self.assertRaises(ValueError):
            battle_camera(program)


if __name__ == "__main__":
    unittest.main()
