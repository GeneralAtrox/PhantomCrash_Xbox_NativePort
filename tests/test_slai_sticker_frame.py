"""The S.L.A.I. body sticker, set in the frame of the body node it lies on."""

import base64
import struct
import unittest

import numpy as np

from phantomcrash_research.slai.native_geometry import (flat_model, nearest_node, sticker_frame,
                                                        world_points)
from phantomcrash_research.slai.pod import Batch, Material, Node


def translation(x, y, z):
    return (1, 0, 0, 0, 0, 1, 0, 0, 0, 0, 1, 0, x, y, z, 1)


def quad(z):
    batch = Batch([(0, 0, z), (1, 0, z), (0, 1, z), (1, 1, z)], [(0, 0, -1)] * 4,
                  [(1, 1, 1, 1)] * 4, [(0, 0), (1, 0), (0, 1), (1, 1)], [(0, 1, 2), (2, 1, 3)])
    return Material(flags=5, colour=(1, 1, 1), batch=batch)


def positions(document):
    accessor = document["accessors"][document["meshes"][0]["primitives"][0]["attributes"]["POSITION"]]
    view = document["bufferViews"][accessor["bufferView"]]
    data = base64.b64decode(document["buffers"][0]["uri"].split(",", 1)[1])
    start = view.get("byteOffset", 0) + accessor.get("byteOffset", 0)
    values = struct.unpack_from(f"<{accessor['count'] * 3}f", data, start)
    return np.array(values).reshape(-1, 3)


# A body: a root 30 up (y down), a hull 5 behind it holding a quad, and a
# head with a quad far away.
BODY = [Node(-1, translation(0, -30, 0)),
        Node(0, translation(0, 0, 5), [quad(0)]),
        Node(0, translation(0, -10, -20), [quad(0)])]
# Its sticker: the same root, the sticker a little behind the hull's quad.
CARRIER = [Node(-1, translation(0, -30, 0)), Node(0, translation(0, 0, 5.1), [quad(0)])]


class StickerFrameTests(unittest.TestCase):
    def test_the_sticker_belongs_to_the_node_it_lies_on(self):
        self.assertEqual(nearest_node(BODY, world_points(CARRIER)), 1)

    def test_without_a_frame_the_root_origin_stays_the_origin(self):
        flat = positions(flat_model(CARRIER, ""))
        # 0.1 units a source unit; y is reversed.
        self.assertTrue(np.allclose(flat[0], [0, 0, 0.51]))

    def test_in_the_hull_frame_the_sticker_sits_just_off_the_hull(self):
        hull = np.array(translation(0, -30, 5), float).reshape(4, 4)
        local = positions(flat_model(CARRIER, "", frame=hull))
        self.assertTrue(np.allclose(local[0], [0, 0, 0.01]))
        self.assertTrue(np.allclose(local[3], [0.1, -0.1, 0.01]))

    def test_roots_are_matched_before_the_node_is_chosen(self):
        # KHT: the body file's root stands 30 lower than its carrier's.
        body = [Node(-1, translation(0, 0, 0)), *BODY[1:]]
        node, frame = sticker_frame(CARRIER, body)
        self.assertEqual(node, 1)
        local = positions(flat_model(CARRIER, "", frame=frame))
        self.assertTrue(np.allclose(local[0], [0, 0, 0.01]))


if __name__ == "__main__":
    unittest.main()
