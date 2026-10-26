"""S.L.A.I.'s Z/PATH routes as port enemy paths, and starts taken from them."""

import struct
import unittest

import numpy as np

from phantomcrash_research.slai.stage_routes import read_route, route_file
from phantomcrash_research.slai.stage_spawns import route_spawns
from phantomcrash_research.slai.stages import move_lids


def _route(curves, kinds, links, word=0):
    """A PTH file: count, record offsets, records, then their curves."""
    count = len(curves)
    records = []
    at = 4 + 4 * count
    for following in links:
        records.append(at)
        at += 12 + 4 * len(following)
    curve_at = []
    for controls in curves:
        curve_at.append(at)
        at += 16 + 16 * len(controls)
    raw = struct.pack(f"<i{count}I", count, *records)
    for kind, following, curve in zip(kinds, links, curve_at):
        raw += struct.pack("<2HII", kind, len(following), word, curve)
        raw += struct.pack(f"<{len(following)}I", *(records[i] for i in following))
    for controls in curves:
        raw += struct.pack("<4I", (len(controls) - 1) // 3 + 1, 0, 0, 0)
        for x, y, z in controls:
            raw += struct.pack("<4f", x, y, z, 1)
    return raw


STRAIGHT = [(0, 0, 0), (10, 0, 0), (20, 0, 0), (30, 0, 0)]
BACK = [(30, 0, 0), (30, 0, -10), (30, 0, -20), (30, 0, -30)]


class StageRouteTests(unittest.TestCase):
    def test_reads_records_links_and_curves_in_port_space(self):
        raw = _route([STRAIGHT, BACK], [0, 6], [[1], [0, 1]], word=0xDEADBEEF)
        route = read_route(raw, scale=0.5, spacing=4.0)
        first, second = route["segments"]
        self.assertEqual((first["kind"], first["next"]), (0, [1]))
        self.assertEqual((second["kind"], second["next"]), (6, [0, 1]))
        # 30 units long, scaled to 15: four points about 4 apart past the start.
        self.assertEqual(first["points"][0], [0, 0, 0])
        self.assertEqual(first["points"][-1], [15, 0, 0])
        self.assertEqual(len(first["points"]), 5)
        # PS2 z runs the other way; y is flipped as well.
        self.assertEqual(second["points"][-1], [15, 0, 15])

    def test_rejects_a_link_to_no_record(self):
        raw = bytearray(_route([STRAIGHT], [0], [[0]]))
        struct.pack_into("<I", raw, 4 + 4 + 12, 0x999)
        with self.assertRaises(ValueError):
            read_route(bytes(raw), scale=1.0)

    def test_route_file_follows_the_city_table(self):
        self.assertEqual(route_file("Z/STAGE/DAY_NEWYORK.BIN"), "PATH020.PTH")
        self.assertEqual(route_file("Z/STAGE/NIGHT_TOKYO2.BIN"), "PATH000.PTH")
        self.assertIsNone(route_file("Z/STAGE/MOON.BIN"))

    def test_starts_lie_on_the_route_far_from_where_robots_come_in(self):
        route = read_route(_route([STRAIGHT, BACK], [0, 1], [[1], [0]]), scale=1.0)
        floor = [{"points": [[-50, 0, -50], [50, 0, 50], [50, 0, -50]]},
                 {"points": [[-50, 0, -50], [-50, 0, 50], [50, 0, 50]]}]
        player, starts = route_spawns(route, floor, count=3)
        self.assertEqual(player, starts[0])
        self.assertEqual(player, [30, 0.05, 30])
        self.assertEqual(len(starts), 3)
        points = {tuple(p) for s in route["segments"] for p in s["points"]}
        for x, y, z in starts:
            self.assertIn((x, 0, z), points)


def _quad(low, high, y):
    (x0, z0), (x1, z1) = low, high
    return [{"points": [[x0, y, z0], [x1, y, z1], [x1, y, z0]], "surface": 0},
            {"points": [[x0, y, z0], [x0, y, z1], [x1, y, z1]], "surface": 0}]


class ArenaLidTests(unittest.TestCase):
    def test_a_lid_over_the_arena_becomes_a_ceiling(self):
        collision = {"floor": _quad((-50, -50), (50, 50), 0) + _quad((-5, -5), (5, 5), 10)
                     + _quad((-48, -48), (48, 48), 30), "walls": []}
        self.assertEqual(move_lids(collision), 2)
        self.assertEqual(len(collision["floor"]), 4)
        for triangle in collision["walls"]:
            a, b, c = np.array(triangle["points"], dtype=float)
            self.assertLess(np.cross(b - a, c - a)[1], 0)  # facing down
        self.assertEqual(move_lids(collision), 0)  # repeatable

    def test_a_high_platform_stays_floor(self):
        collision = {"floor": _quad((-50, -50), (50, 50), 0) + _quad((-5, -5), (5, 5), 10),
                     "walls": []}
        self.assertEqual(move_lids(collision), 0)
        self.assertEqual(len(collision["floor"]), 4)


if __name__ == "__main__":
    unittest.main()
