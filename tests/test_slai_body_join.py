"""The original joins every body to every legs without a per-pairing rule."""
from pathlib import Path
import struct
import unittest

from phantomcrash_research.slai.archive import SlaiDisc
from phantomcrash_research.slai.program import ProgramData
from phantomcrash_research.slai.body_join import body_join_contract


class BodyJoinTests(unittest.TestCase):
    def setUp(self):
        image = Path('C:/Projects/SLAI/SLAI.iso')
        if not image.exists():
            self.skipTest('Local user disc is unavailable')
        self.disc = SlaiDisc(image)
        self.disc.__enter__()
        self.addCleanup(self.disc.__exit__, None, None, None)
        self.program = ProgramData(self.disc)

    def test_every_pairing_uses_the_first_leg_attachment_at_unit_scale(self):
        result = body_join_contract(self.program)
        self.assertEqual(result['body_scale'], 1.0)
        self.assertEqual(result['body_offset'], [0.0, 0.0, 0.0])
        self.assertIsNone(result['per_pairing_rule'])

    def changed(self, changes):
        base = self.program

        class ChangedProgram:
            def read(self, location, size):
                if location in changes:
                    return struct.pack('<I', changes[location])
                return base.read(location, size)
        return ChangedProgram()

    def test_a_changed_link_or_scale_is_rejected(self):
        for address in (0x2689b0, 0x2689b4, 0x2689bc, 0x1b35b8, 0x2666c4, 0x4dc908):
            with self.assertRaises(ValueError):
                body_join_contract(self.changed({address: 0}))

    def test_a_placement_call_in_the_assembly_path_is_rejected(self):
        # jal 0x1ca900 (translation) where the assembly calls nothing.
        with self.assertRaises(ValueError):
            body_join_contract(self.changed({0x268e00: 0x0c072a40}))
        # A non-unit scale before a scale call.
        with self.assertRaises(ValueError):
            body_join_contract(self.changed({0x268aac: 0x3c023fc0}))


if __name__ == '__main__':
    unittest.main()
