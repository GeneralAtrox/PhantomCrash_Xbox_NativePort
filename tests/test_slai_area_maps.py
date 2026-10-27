"""Blueprint maps of S.L.A.I.'s stages for RUMBLING's area screen (slai/area_maps.py)."""

import math
import unittest

import numpy as np

from phantomcrash_research.slai.area_maps import (FRAME, MARGIN, area_map, fit, frame_of, placed,
                                                  segments)


def square_floor(size: float, height: float = 0.0, at=(0.0, 0.0)) -> list[dict]:
    x, z = at
    a, b = [x, height, z], [x + size, height, z]
    c, d = [x + size, height, z + size], [x, height, z + size]
    return [{"points": [a, b, c], "surface": 1}, {"points": [a, c, d], "surface": 1}]


def wall(start, end, base: float, top: float) -> list[dict]:
    (ax, az), (bx, bz) = start, end
    return [{"points": [[ax, base, az], [bx, base, bz], [bx, top, bz]], "surface": 0},
            {"points": [[ax, base, az], [bx, top, bz], [ax, top, az]], "surface": 0}]


def arena(*inner) -> dict:
    """A 40-unit square floor inside its four walls, with more walls."""
    walls = []
    for start, end in [((0, 0), (40, 0)), ((40, 0), (40, 40)), ((40, 40), (0, 40)),
                       ((0, 40), (0, 0))] + list(inner):
        walls += wall(start, end, 0.0, 5.0)
    return {"floor": square_floor(40.0), "walls": walls}


class AreaMapTests(unittest.TestCase):
    def test_placed_matches_rumbling_maps_turn_about_the_pivot(self):
        # rumbling_map.cpp: z runs up the map, then the turn about the pivot.
        robot_map = {"offset": [10.0, 20.0], "pivot": [0.0, 0.0], "angle": 0.0, "scale": 2.0}
        self.assertEqual(placed(robot_map, 3.0, 4.0), (16.0, 12.0))
        turned = dict(robot_map, angle=math.pi / 2)
        u, v = placed(turned, 3.0, 4.0)
        self.assertAlmostEqual(u, 12.0)
        self.assertAlmostEqual(v, -16.0)

    def test_floors_outline_their_edge_and_walls_their_run(self):
        lines = segments(arena(((10, 10), (30, 10))))
        outline = [line for line in lines if line["outline"]]
        self.assertEqual(len(outline), 4)  # the square's sides, not its diagonal
        walls = [line for line in lines if not line["outline"]]
        # The inner wall's two triangles are one run; the four outer walls
        # stand on the outline and are drawn as it.
        self.assertEqual(len(walls), 1)
        self.assertEqual(walls[0]["top"], 5.0)

    def test_props_kerbs_and_slopes_are_left_out(self):
        collision = arena()
        collision["floor"] += square_floor(1.0, 3.0, at=(5.0, 5.0))
        collision["walls"] += wall((10, 10), (30, 10), 0.0, 0.1)
        collision["walls"] += [{"points": [[0, 0, 0], [10, 0, 0], [10, 10, 10]], "surface": 0}]
        self.assertEqual(len(segments(collision)), len(segments(arena())))

    def test_ground_far_past_the_walls_is_left_out(self):
        collision = arena()
        collision["floor"] += square_floor(30.0, 0.0, at=(500.0, 500.0))
        self.assertEqual(len(segments(collision)), len(segments(arena())))

    def test_the_fit_fills_the_frame_inside_its_margin(self):
        points = np.array([[0.0, 0.0], [100.0, 0.0], [100.0, 50.0], [0.0, 50.0]])
        robot_map = fit(points)
        placed_points = np.array([placed(robot_map, x, z) for x, z in points])
        self.assertGreaterEqual(placed_points.min(), MARGIN - 1e-3)
        self.assertLessEqual(placed_points[:, 0].max(), FRAME[0] - MARGIN + 1e-3)
        self.assertLessEqual(placed_points[:, 1].max(), FRAME[1] - MARGIN + 1e-3)
        self.assertAlmostEqual(robot_map["angle"], 0.0)  # no turn needed

    def test_frames_add_outline_then_buildings_then_height(self):
        self.assertEqual(frame_of({"outline": True, "top": 0.5, "base": 0.5}, 0.0, 10.0), 0)
        self.assertEqual(frame_of({"outline": False, "top": 8.0, "base": 0.0}, 0.0, 10.0), 1)
        self.assertEqual(frame_of({"outline": True, "top": 6.0, "base": 6.0}, 0.0, 10.0), 2)
        self.assertEqual(frame_of({"outline": False, "top": 20.0, "base": 12.0}, 0.0, 10.0), 3)

    def test_an_area_map_has_four_frames_and_its_gates_on_screen(self):
        sheet, entry = area_map(arena(), [[20.0, 0.0, 20.0]], "textures/slai/maps/x.png", "x")
        self.assertEqual(len(entry["map_frames"]), 4)
        self.assertEqual(entry["map_frames"][3]["size"], list(FRAME))
        self.assertEqual(sheet.size, (FRAME[0] * 4, FRAME[1] * 4))
        (gate_x, gate_y), = entry["gates"]
        # The square's middle is the frame's middle, on the screen.
        self.assertAlmostEqual(gate_x, 234 + FRAME[0] / 2, delta=0.1)
        self.assertAlmostEqual(gate_y, 73 + FRAME[1] / 2, delta=0.1)
        self.assertGreater(np.array(sheet)[..., 3].max(), 0)  # something is drawn


if __name__ == "__main__":
    unittest.main()
