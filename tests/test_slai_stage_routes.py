"""S.L.A.I.'s Z/PATH routes as port enemy paths, and starts taken from them."""

import struct
import unittest
from types import SimpleNamespace
from unittest import mock

import numpy as np

from phantomcrash_research.slai.stage_routes import read_route, route_file
from phantomcrash_research.slai.stage_spawns import route_spawns
from phantomcrash_research.slai import stages
from phantomcrash_research.slai.pod import Batch, Material, Node
from phantomcrash_research.slai.stages import move_lids, two_sided


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
        # PS2 y points down; x and z keep their sense in the port.
        self.assertEqual(second["points"][-1], [15, 0, -15])

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
        self.assertEqual(player, [30, 0.05, -30])
        self.assertEqual(len(starts), 3)
        points = {tuple(p) for s in route["segments"] for p in s["points"]}
        for x, y, z in starts:
            self.assertIn((x, 0, z), points)


def _quad(low, high, y):
    (x0, z0), (x1, z1) = low, high
    return [{"points": [[x0, y, z0], [x1, y, z1], [x1, y, z0]], "surface": 0},
            {"points": [[x0, y, z0], [x0, y, z1], [x1, y, z1]], "surface": 0}]


class CollisionWindingTests(unittest.TestCase):
    def test_a_standing_surface_faces_up_as_phantom_crashs_do(self):
        # A source floor triangle, clockwise once y is turned up.
        batch = Batch([(0, 0, 0), (10, 0, 0), (0, 0, 10)], [(0, -1, 0)] * 3, [(1, 1, 1, 1)] * 3,
                      [(0, 0)] * 3, [(0, 1, 2)])
        nodes = [Node(-1, (1, 0, 0, 0, 0, 1, 0, 0, 0, 0, 1, 0, 0, 0, 0, 1),
                      [Material(flags=0, colour=(1, 1, 1), batch=batch)])]
        disc = SimpleNamespace(read=lambda file: b"")
        file = SimpleNamespace(name="FLOOR.PO2", package=0)
        with mock.patch.object(stages, "read_pod", return_value=nodes):
            collision = stages._collision(disc, [file])
        self.assertEqual(len(collision["floor"]), 1)
        a, b, c = np.array(collision["floor"][0]["points"], dtype=float)
        self.assertGreater(np.cross(b - a, c - a)[1], 0)


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


def _facing(triangle):
    a, b, c = np.array(triangle["points"], dtype=float)
    normal = np.cross(b - a, c - a)
    return normal / np.linalg.norm(normal)


class TwoSidedTests(unittest.TestCase):
    def test_a_tunnel_modelled_facing_out_carries_and_walls_from_inside(self):
        ground = _quad((-50, -50), (50, 50), 10) + _quad((20, 20), (30, 30), 20)  # and a stand
        # Egypt's tunnel: its floor faces down, its side wall faces away.
        tunnel = [{"points": [t["points"][0], t["points"][2], t["points"][1]], "surface": 0}
                  for t in _quad((-5, -50), (5, 50), -10)]
        side = {"points": [[-5, -10, -50], [-5, 0, -50], [-5, 0, 50]], "surface": 0}
        collision = {"floor": ground, "walls": tunnel + [side]}
        self.assertEqual(two_sided(collision), 7)
        self.assertEqual(len(collision["floor"]), 6)
        self.assertTrue(all(_facing(t)[1] > 0 for t in collision["floor"]))
        ceilings = [t for t in collision["walls"] if _facing(t)[1] < -0.5]
        self.assertEqual(len(ceilings), 6)  # under the ground, stand and tunnel floor
        sides = sorted(_facing(t)[0] for t in collision["walls"] if abs(_facing(t)[1]) < 0.5)
        self.assertEqual(sides, [-1, 1])
        self.assertEqual(two_sided(collision), 0)  # repeatable
        self.assertEqual(len(collision["walls"]), 8)

    def test_an_arena_lid_stays_a_ceiling_only(self):
        collision = {"floor": _quad((-50, -50), (50, 50), 0) + _quad((-48, -48), (48, 48), 30),
                     "walls": []}
        two_sided(collision)
        two_sided(collision)
        heights = sorted(round(t["points"][0][1]) for t in collision["walls"])
        self.assertEqual(heights, [0, 0, 30, 30])
        self.assertEqual(len(collision["floor"]), 2)
        self.assertEqual(collision["extras"]["ceiling_triangles"], 2)


if __name__ == "__main__":
    unittest.main()
