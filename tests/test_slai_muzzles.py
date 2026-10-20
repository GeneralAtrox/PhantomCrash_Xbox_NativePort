"""Recovered S.L.A.I. muzzle nodes, flash table, flash scale and sockets."""

import struct
import unittest
from unittest import mock

from phantomcrash_research.slai import muzzles


class Program:
    def __init__(self, guards=muzzles.GUARDS):
        self.data = {address: struct.pack("<I", word) for address, word in guards.items()}
        self.data[muzzles.BASE_SCALES] = struct.pack("<4f", 0.9, 0.9, 1.1, 1.1)

    def read(self, address, size):
        for start, value in self.data.items():
            if start <= address and address + size <= start + len(value):
                return value[address - start:address - start + size]
        if size <= 4:
            return bytes(size)
        # Adjacent guard words (lui/ori pairs) are stored separately.
        return b"".join(self.read(address + i, 4) for i in range(0, size, 4))


class Disc:
    def __init__(self, files):
        self.files = files

    def find(self, name):
        return [name]

    def read(self, name):
        return self.files[name]


def row(index, model, muzzle_nodes):
    callbacks = [""] * 51
    for node in muzzle_nodes:
        callbacks[node] = hex(muzzles.MUZZLE_CALLBACK)
    callbacks[1] = "0x4e6da0"
    return {"index": index, "model": model, "node_callbacks": callbacks}


class FlashTableTests(unittest.TestCase):
    def test_entries_are_counted_node_lists(self):
        self.assertEqual(muzzles.read_flash_nodes(bytes([1, 2, 2, 2, 3, 0]), 3), [[2], [2, 3], []])

    def test_rejects_trailing_bytes_and_long_entries(self):
        with self.assertRaises(ValueError):
            muzzles.read_flash_nodes(bytes([1, 2, 9]), 1)
        with self.assertRaises(ValueError):
            muzzles.read_flash_nodes(bytes([5, 1, 2, 3, 4, 5]), 1)

    def test_hit_scales_are_interpolation_records(self):
        raw = struct.pack("<2i2f", 50, 350, 0.08, 0.2)
        self.assertEqual(muzzles.read_hit_scales(raw),
                         [{"low": 50, "high": 350, "low_scale": 0.08, "high_scale": 0.2}])

    def test_multiplier_lists(self):
        self.assertEqual(muzzles.flash_multiplier(0x30, 1.3, 1.7), 1.3)
        self.assertEqual(muzzles.flash_multiplier(0x5a, 1.3, 1.7), 1.7)
        self.assertEqual(muzzles.flash_multiplier(2, 1.3, 1.7), 1.0)


class DecodeTests(unittest.TestCase):
    def decode(self, program):
        tables = {"arms": [row(0, "S3A01H.PO2", [4]), row(1, "S3A02H.PO2", [7, 8])],
                  "shoulders": [row(0, "S3S01H.PO2", [4]), row(1, "S3S02H.PO2", [])]}
        disc = Disc({muzzles.FLASH_TABLE: bytes([1, 2, 2, 2, 3, 1, 3, 0]),
                     muzzles.HIT_SCALE_TABLE: struct.pack("<2i2f", 1, 2, 0.5, 1.0)})
        with mock.patch.object(muzzles, "decode_motion_resources", return_value={"tables": tables}):
            return muzzles.decode_muzzles(program, disc)

    def test_descriptor_order_ids_and_scales(self):
        result = self.decode(Program())
        weapons = result["weapons"]
        self.assertEqual([w["source_id"] for w in weapons], [0, 1, 0x30, 0x31])
        self.assertEqual([w["flash_nodes"] for w in weapons], [[2], [2, 3], [3], []])
        self.assertEqual([w["muzzle_nodes"] for w in weapons], [[4], [7, 8], [4], []])
        self.assertEqual([w["barrel_flash"] for w in weapons], [False, True, False, False])
        self.assertEqual([w["flash_scale"] for w in weapons], [1.17, 1.17, 1.43, 1.1])
        self.assertEqual([layer["alpha"] for layer in result["flash"]["layers"]], [0.4, 0.8])
        self.assertEqual(result["flash"]["visible_updates"], 1)

    def test_other_executables_are_rejected(self):
        guards = dict(muzzles.GUARDS)
        guards[0x51175c] = 0
        with self.assertRaises(ValueError):
            self.decode(Program(guards))


class SocketTests(unittest.TestCase):
    model = {"scene": 0, "scenes": [{"nodes": [3]}], "nodes": [
        {"translation": [0.0, 0.5, 0.0], "children": [1]},
        {"translation": [0.0, 0.0, -2.0]},
        {"translation": [9.0, 9.0, 9.0]},
        {"scale": [-1.0, 1.0, 1.0], "translation": [0.2, 0.0, 0.0], "children": [0]}]}

    def test_rest_position_under_mounts_with_game_z(self):
        mount = muzzles.muzzle_mount(self.model, [1], {"resource": 7, "muzzles": []}, True)
        self.assertEqual(mount["resource"], 7)
        socket = mount["muzzles"][0]
        self.assertEqual(socket["position"], [0.2, 0.5, 2.0])
        self.assertEqual((socket["node"], socket["bone"]), (1, 0))
        self.assertEqual(len(socket["chain"]), 1)

    def test_shoulders_have_no_bone_and_empty_lists_keep_the_template(self):
        mount = muzzles.muzzle_mount(self.model, [0], {}, False)
        self.assertEqual(mount["muzzles"][0]["bone"], -1)
        self.assertIsNone(muzzles.muzzle_mount(self.model, [], {}, True))


if __name__ == "__main__":
    unittest.main()
