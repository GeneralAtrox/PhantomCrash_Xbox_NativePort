"""The S.L.A.I. battle camera block comes from the view-screen setup and the
behind view 0x260070."""

import struct
import unittest

from phantomcrash_research.slai.battle_camera import (
    AIR_EASE_LUI, AIR_EASE_ORI, AIR_GUARDS, AIR_LOOK_SCALE_LUI, AIR_RISE_LUI, AIR_UPDATES_LUI,
    ASPECT_X_ADDRESS, ASPECT_Y_ADDRESS, CHASE_GUARDS, CHASE_ROWS, CHASE_TABLE,
    EYE_BACK_LUI, EYE_RISE_LUI, LOOK_AHEAD_LUI, LOOK_RISE_LUI, SCREEN_DISTANCE_ADDRESS,
    MIN_EYE_LUI, VIEW_SCREEN_GUARDS, WALL_AIR_CLIMB_LUI, WALL_AIR_RISE_LUI, WALL_EASE_LUI,
    WALL_GUARDS, WALL_MARGIN_LUI, WALL_REACH_LUI, WALL_NORMAL_LUI, WALL_RISE_LUI,
    WALL_ROW_RISE_LUI, battle_camera, chase_rows, leg_chase_camera)


def lui_ori(addresses, value):
    word = struct.unpack("<I", struct.pack("<f", value))[0]
    return {addresses[0]: struct.pack("<I", 0x3C020000 | word >> 16),
            addresses[1]: struct.pack("<I", 0x34420000 | word & 0xFFFF)}


def lui(value):
    return struct.pack("<I", 0x3C020000 | struct.unpack("<I", struct.pack("<f", value))[0] >> 16)


class Program:
    def __init__(self):
        guards = {**VIEW_SCREEN_GUARDS, **CHASE_GUARDS, **AIR_GUARDS, **WALL_GUARDS}
        self.data = {a: struct.pack("<I", w) for a, w in guards.items()}
        self.data.update({SCREEN_DISTANCE_ADDRESS: struct.pack("<f", 549.0),
                          ASPECT_X_ADDRESS: struct.pack("<f", 1.0),
                          ASPECT_Y_ADDRESS: struct.pack("<f", 1.16),
                          EYE_RISE_LUI: lui(4.5), EYE_BACK_LUI: lui(12.0),
                          LOOK_RISE_LUI: lui(-0.5), LOOK_AHEAD_LUI: lui(20.0),
                          AIR_UPDATES_LUI: lui(4.0), AIR_RISE_LUI: lui(2.0),
                          AIR_LOOK_SCALE_LUI: lui(0.75),
                          AIR_EASE_LUI: struct.pack("<I", 0x3C033D4C),
                          AIR_EASE_ORI: struct.pack("<I", 0x3463CCCD),
                          WALL_RISE_LUI: lui(0.25), WALL_REACH_LUI: lui(10.0),
                          WALL_AIR_RISE_LUI: lui(-0.5), WALL_AIR_CLIMB_LUI: lui(2.5),
                          **lui_ori(MIN_EYE_LUI, 1.2), **lui_ori(WALL_ROW_RISE_LUI, 0.35),
                          **lui_ori(WALL_MARGIN_LUI, 0.15), **lui_ori(WALL_NORMAL_LUI, 0.15),
                          **lui_ori(WALL_EASE_LUI, 1 / 3)})
        # Shipped rows 0 and 19; the rest repeat row 0.
        rows = [(0.0, -0.6, 3.3)] * CHASE_ROWS
        rows[19] = (0.0, -0.9, 2.5)
        self.data[CHASE_TABLE] = b"".join(struct.pack("<3f", *r) for r in rows)

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
                         {"eye_height": 5.0, "behind": 8.6, "height": 0.5, "look_ahead": 20.0,
                          "air": {"rise": 2.0, "look_scale": 0.75, "ease": 0.05, "updates": 4.0},
                          "wall": {"margin": 0.15, "reach": 10.0, "normal_limit": 0.15,
                                   "ease": 0.333333, "rise": 0.25, "air_rise": 0.5,
                                   "air_climb": 2.5}})

    def test_the_legs_row_places_the_eye(self):
        rows = chase_rows(Program())
        self.assertEqual(len(rows), CHASE_ROWS)
        # Row 19 (KHT-1's legs): (0, -0.9 - 4.5, 2.5 - 12), source y down;
        # PCSX2 showed its eye 5.395 up while a wall cut the distance.
        camera = leg_chase_camera(rows, {"index": 7, "alternate_model": 19})
        self.assertEqual((camera["behind"], camera["eye_height"]), (9.5, 5.4))
        self.assertEqual(camera["provenance"]["row"], 19)
        self.assertNotIn("wall_rise", camera)

    def test_rows_31_and_33_carry_their_own_stage_response(self):
        rows = chase_rows(Program())
        self.assertEqual(rows[31]["wall_rise"], 0.35)
        self.assertEqual(rows[33]["min_eye_height"], 1.2)
        self.assertNotIn("min_eye_height", rows[31])
        camera = leg_chase_camera(rows, {"index": 2, "alternate_model": 33})
        self.assertEqual(camera["min_eye_height"], 1.2)

    def test_a_legs_record_beyond_the_table_is_refused(self):
        with self.assertRaises(ValueError):
            leg_chase_camera(chase_rows(Program()), {"index": 0, "alternate_model": CHASE_ROWS})

    def test_a_sideways_eye_is_refused(self):
        program = Program()
        program.data[CHASE_TABLE] = struct.pack("<3f", 1.0, 0.0, 0.0) * CHASE_ROWS
        with self.assertRaises(ValueError):
            chase_rows(program)

    def test_a_different_setup_is_refused(self):
        for address in [*VIEW_SCREEN_GUARDS, *CHASE_GUARDS, *AIR_GUARDS, *WALL_GUARDS]:
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
