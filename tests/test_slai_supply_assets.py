import struct
import unittest
from pathlib import Path

from phantomcrash_research.slai.archive import SlaiDisc
from phantomcrash_research.slai.program import ProgramData
import math

from phantomcrash_research.slai.supply_assets import (FLIGHT_GUARDS, read_supply_path, supply_assets,
                                                     supply_flight, stage_supply_routes)
from phantomcrash_research.slai.world_units import world_units


class SupplyAssetTests(unittest.TestCase):
    def test_curve_header_and_axes(self):
        header = struct.pack('<5h2xf4x', 1, 0, 0, 0, 0, 3.5)
        raw = header + struct.pack('<16f', *([1, 2, 3, 1] * 4))
        path = read_supply_path(raw, 2)
        self.assertEqual(path['segments'], [[[2, -4, 6]] * 4])
        self.assertEqual(path['source_header_float'], 3.5)
        self.assertEqual(path['floor'], -7)  # source y points down
        self.assertEqual(read_supply_path(bytes(20), 2)['floor'], 0.0)
        with self.assertRaises(ValueError):
            read_supply_path(raw[:-1], 2)
        self.assertEqual(read_supply_path(bytes(20), 2)['segments'], [])

    def test_current_package_routes_and_period_aliases(self):
        image = Path('C:/Projects/SLAI/SLAI.iso')
        if not image.is_file():
            self.skipTest('Source disc unavailable')
        with SlaiDisc(image) as disc:
            program = ProgramData(disc)
            units = world_units(program)
            scale = units['world_scale']
            assets = supply_assets(disc, program, units)
        self.assertEqual(sum(len(rows) for rows in assets['routes'].values()), 18)
        self.assertEqual(sum(len(r['segments']) for rows in assets['routes'].values() for r in rows), 489)
        self.assertEqual(stage_supply_routes(assets, 'DAY_TOKYO2.BG'),
                         stage_supply_routes(assets, 'NIGHT_TOKYO.BG'))
        self.assertEqual(len(stage_supply_routes(assets, 'DAY_ENGLAND.BG')), 3)
        self.assertEqual(len(stage_supply_routes(assets, 'PRACTICE.BG')), 1)
        self.assertEqual(stage_supply_routes(assets, 'MOTION.BG'), [])
        self.assertEqual(assets['pulse']['colour_steps'], 96)
        self.assertEqual(assets['pulse']['frame_rate'], 30)
        london = stage_supply_routes(assets, 'DAY_ENGLAND.BG')
        self.assertEqual([round(r['source_header_float'], 3) for r in london], [5, 10, 25])
        for row in london:
            self.assertEqual(row['floor'], -row['source_header_float'] * scale)
        flight = assets['flight']
        self.assertEqual(flight['update_rate'], 30)
        self.assertEqual(flight['respawn'], 30)
        self.assertAlmostEqual(flight['speed'], .015, 6)
        self.assertEqual((flight['release_delay'], flight['lifetime'], flight['burst_hold']),
                         (100, 900, 15))
        self.assertEqual(flight['wreck_updates'], 500)
        self.assertAlmostEqual(flight['hatch_open'], math.pi / 4, 6)
        self.assertAlmostEqual(flight['wreck_accel'], 1.01, 6)
        self.assertAlmostEqual(flight['wreck_shade'], .4, 6)
        self.assertAlmostEqual(flight['wreck_keep'], .6, 6)
        self.assertEqual(flight['joint_controls'], [12, 16, 9, 13, 10, 14, 0])
        # Joint 0 swings (57, -37, -11) degrees in source axes, mirrored joint 1.
        self.assertEqual([round(math.degrees(v), 3) for v in flight['wing_swing'][0]], [57, 37, 11])
        self.assertEqual([round(math.degrees(v), 3) for v in flight['wing_swing'][1]], [57, -37, -11])
        self.assertAlmostEqual(flight['wreck_pose'][1][1], -.61087, 4)
        self.assertEqual(len(flight['wreck_pose']), 7)

    def test_flight_refuses_an_unexpected_program(self):
        class Program:
            def __init__(self, changed):
                self.changed = changed

            def read(self, address, size):
                word = FLIGHT_GUARDS[address] ^ (address == self.changed)
                return struct.pack('<I', word) if address in FLIGHT_GUARDS else bytes(size)

        with self.assertRaises(ValueError):
            supply_flight(Program(0x5240e0), 1)
