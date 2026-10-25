"""S.L.A.I. part paint sheets, including parts that wear another part's."""

import unittest

from phantomcrash_research.slai.native_parts import part_paints

BODY = [{"source_colour": 1, "file": "textures/s9b01h01.png"},
        {"source_colour": 2, "file": "textures/s9b01h02.png"}]


class PartPaintsTest(unittest.TestCase):
    def test_a_part_keeps_its_own_sheets(self):
        paints = {"S9B01H": BODY, "S1L01H": [{"source_colour": 1, "file": "textures/s1l01h01.png"}]}
        model = {"images": [{"uri": "../textures/s9b01h01.png"}]}
        self.assertEqual(part_paints(paints, "S1L01H", model), paints["S1L01H"])

    def test_legs_without_sheets_wear_the_set_their_texture_belongs_to(self):
        # DUSTER-3Wheel (S9L01H) has no sheets and textures itself with its body's.
        paints = {"S9B01H": BODY, "S9L01H": []}
        model = {"images": [{"uri": "../textures/s9b01h01.png"}]}
        self.assertEqual(part_paints(paints, "S9L01H", model), BODY)

    def test_no_sheet_and_no_match_leaves_the_part_unpainted(self):
        model = {"images": [{"uri": "../textures/other.png"}]}
        self.assertEqual(part_paints({"S9B01H": BODY}, "SAL01H", model), [])


if __name__ == "__main__":
    unittest.main()
