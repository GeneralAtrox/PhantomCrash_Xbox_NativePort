"""S.L.A.I. arm aim rows and gun flight, from synthetic program bytes."""

import struct
import unittest

from phantomcrash_research.slai.arm_aim import (ARM_AIM_TABLE, GUARDS, ROW_BYTES, ROWS,
                                                arm_aim_rows, leg_arm_aim, weapon_nodes)
from phantomcrash_research.slai.native_parts import native_flight


class FakeProgram:
    def __init__(self, table, guards=GUARDS):
        self.memory = {address: struct.pack("<I", word) for address, word in guards.items()}
        self.table = table

    def read(self, va, size, overlay="battledvd.bin"):
        if va == ARM_AIM_TABLE:
            return self.table[:size]
        return self.memory[va][:size]


def table_with_row(index, values, order, keep, twist):
    raw = bytearray(ROW_BYTES * ROWS)
    at = index * ROW_BYTES
    struct.pack_into("<10f", raw, at, *values)
    raw[at + 0x28:at + 0x2B] = bytes(order).ljust(3, b"\0")
    raw[at + 0x2B] = keep
    struct.pack_into("<f", raw, at + 0x2C, twist)
    return bytes(raw)


class ArmAimTests(unittest.TestCase):
    def test_rows_split_by_slot_with_the_twist_negated_for_slot_zero(self):
        values = (0.5, 0.25, 0.125, -1.0, 1.0, 1.5, -0.25, -0.125, -0.5, 0.75)
        rows = arm_aim_rows(FakeProgram(table_with_row(3, values, (3, 1, 2), 0, 0.5)))
        self.assertEqual(len(rows), ROWS)
        row = rows[3]
        slot0, slot1 = row["sides"]
        self.assertEqual((slot0["node"], slot1["node"]), (4, 2))
        self.assertEqual((slot0["pitch"], slot0["yaw"], slot0["roll"]), (1.5, -0.25, -0.125))
        self.assertEqual(slot0["pitch_limits"], [-0.5, 0.75])
        self.assertEqual((slot1["pitch"], slot1["pitch_limits"]), (0.5, [-1.0, 1.0]))
        self.assertEqual((slot0["twist"], slot1["twist"]), (-0.5, 0.5))
        self.assertEqual(row["order"], ["z", "x", "y"])
        self.assertFalse(row["keep_rotation"])

    def test_unknown_code_is_refused(self):
        guards = {**GUARDS, 0x4E7B00: 0}
        with self.assertRaises(ValueError):
            arm_aim_rows(FakeProgram(bytes(ROW_BYTES * ROWS), guards))

    def test_legs_pick_their_row_by_the_record_and_weapons_their_nodes(self):
        rows = arm_aim_rows(FakeProgram(table_with_row(7, (0,) * 10, (1,), 1, 0)))
        self.assertEqual(leg_arm_aim(rows, {"index": 1, "alternate_model": 7})["row"], 7)
        with self.assertRaises(ValueError):
            leg_arm_aim(rows, {"index": 1, "alternate_model": ROWS})
        profile = {"node_callbacks": ["", "0x4e6dc0", "0x4e6db0", None, "0x4e6db0"]}
        self.assertEqual(weapon_nodes(profile), (1, [2, 4]))

    def test_guns_fly_at_their_own_record_speeds_and_arm_actions_keep_the_template(self):
        gun = {"start_speed": 3.8, "top_speed": 3.8, "acceleration": 0.0}
        native_flight(gun, {"kind": 2, "start_speed": 100.0, "top_speed": 100.0,
                            "acceleration": 0.5})
        self.assertEqual((gun["start_speed"], gun["top_speed"], gun["acceleration"]),
                         (100.0, 100.0, 0.5))
        blade = {"start_speed": 1.0, "top_speed": 2.0, "acceleration": 0.0}
        native_flight(blade, {"kind": 6, "start_speed": 9.0, "top_speed": 9.0,
                              "acceleration": 9.0})
        self.assertEqual(blade["top_speed"], 2.0)


if __name__ == "__main__":
    unittest.main()
