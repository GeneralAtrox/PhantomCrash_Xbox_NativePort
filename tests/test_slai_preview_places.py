"""S.L.A.I. parts shown alone get a place centred on their model (slai/preview_places.py)."""

import json
import math
import tempfile
import unittest
from pathlib import Path

import numpy as np

from phantomcrash_research.slai.preview_places import centre_places, model_middle, turned


def write_box(root: Path, resource: int, low, high, translation=(0.0, 0.0, 0.0)) -> None:
    gltf = {"nodes": [{"translation": list(translation), "children": [1]}, {"mesh": 0}],
            "meshes": [{"primitives": [{"attributes": {"POSITION": 0}}]}],
            "accessors": [{"min": list(low), "max": list(high)}]}
    path = root / f"models/binlink/{resource}.gltf"
    path.parent.mkdir(parents=True, exist_ok=True)
    path.write_text(json.dumps(gltf), encoding="utf-8")


class PreviewPlacesTests(unittest.TestCase):
    def test_middle_follows_the_node_chain_and_mirrors_z(self):
        with tempfile.TemporaryDirectory() as folder:
            root = Path(folder)
            write_box(root, 1, (0, 0, 0), (2, 4, 6), translation=(1, 0, 0))
            self.assertEqual(model_middle(root / "models/binlink/1.gltf").tolist(), [2.0, 2.0, -3.0])

    def test_turned_matches_the_ports_row_vector_rotations(self):
        # rotation_y(90 degrees) takes x onto -z (math_rotations.cpp).
        point = turned([0, 0, 0, 0, math.pi / 2, 0], np.array([1.0, 0.0, 0.0]))
        self.assertTrue(np.allclose(point, [0.0, 0.0, -1.0]))

    def test_an_imported_type_lands_its_middle_where_its_templates_does(self):
        with tempfile.TemporaryDirectory() as folder:
            root = Path(folder)
            write_box(root, 10, (-1, -1, -1), (1, 1, 1))     # Phantom Crash arm, middle at 0
            write_box(root, 20, (3, 5, -1), (5, 7, 1))       # S.L.A.I. arm, middle at (4, 6, 0)
            place = [0.5, 2.0, -3.0, 0.0, math.pi / 2, 0.0]
            preview = {"lone_part": {"places": [place], "by_type": {"arms": [0, 0]}}}
            catalogue = {"arm_types": [{"models": [10, 11]}, {"models": [20, 21]}]}
            imported = [{"category": "arm", "type": 1}]
            base = {"places": 1, "arms": 1}
            self.assertEqual(centre_places(preview, catalogue, imported, [root], base), 1)
            places = preview["lone_part"]["places"]
            self.assertEqual(preview["lone_part"]["by_type"]["arms"], [0, 1])
            own = places[1]
            self.assertEqual(own[3:], place[3:])
            middle = turned(own, np.array([4.0, 6.0, 0.0])) + own[:3]
            self.assertTrue(np.allclose(middle, place[:3], atol=1e-3))
            # Run again, it keeps the place it has.
            self.assertEqual(centre_places(preview, catalogue, imported, [root], base), 0)


if __name__ == "__main__":
    unittest.main()
