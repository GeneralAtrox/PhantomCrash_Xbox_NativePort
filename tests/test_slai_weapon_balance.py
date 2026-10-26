"""The S.L.A.I. weapon balance pass, on hand-made tables."""

import copy
import unittest

from phantomcrash_research.slai.weapon_balance import (balance_weapons, restore_source, rules,
                                                       scale_curve, value)


def curve(base, top=0, mid=0, knee=0):
    return {"base": base, "mid": mid, "top": top, "knee": knee, "low_scale": -1}


def weapon(kind, damage, blast=0, fire_frames=60, price=1000, weight=100, **extra):
    return {"name": "W", "kind": kind, "fire_frames": fire_frames, "rounds": curve(1),
            "damage": curve(damage, damage), "blast_damage": curve(blast, blast),
            "damage_min": damage // 2, "price": curve(price, price), "weight": curve(weight),
            **extra}


SETTINGS = {"enabled": True, "levels": [0, 99], "rate_kinds": [0], "min_samples": 1,
            "groups": {"damage": {"measure": ["damage", "blast_damage"], "per_second": True,
                                  "cap_to_strongest": True, "scalars": ["damage_min"], "round": 0},
                       "price": {"measure": ["price"], "round": 0}}}


class WeaponBalanceTests(unittest.TestCase):
    def test_value_follows_the_ports_level_curve(self):
        self.assertEqual(value(curve(100, 99), 99), 199)
        self.assertEqual(value(curve(100, 0, 50, 99), 99), 150)
        knee = curve(100, 300, 50, 50)
        self.assertEqual(value(knee, 25), 125)
        self.assertEqual(value(knee, 50), 150)
        self.assertEqual(value(knee, 99), 400)

    def test_a_scaled_curve_meets_its_factor_at_each_end_and_the_knee(self):
        source = curve(100, 300, 50, 33)
        scaled = scale_curve(source, 0.5, 2.0, 2)
        self.assertAlmostEqual(value(scaled, 0), 50)
        self.assertAlmostEqual(value(scaled, 99), 800)
        self.assertAlmostEqual(value(scaled, 33), 150 * (0.5 + 1.5 * 33 / 99), places=1)

    def test_imported_weapons_meet_the_native_median_per_second(self):
        base = {"arm_weapons": [weapon(0, 100), weapon(0, 300)], "shoulder_weapons": []}
        # Twice the native median a second: 400 a hit, every 60 frames, and
        # the same rate from 200 a hit every 30.
        parts = {"arm_weapons": [weapon(0, 400, slai_model="A"), weapon(0, 200, fire_frames=30, slai_model="B"),
                                 weapon(0, 999)], "shoulder_weapons": []}
        report = balance_weapons(parts, base, SETTINGS)
        self.assertEqual(report["arm_weapons"]["damage"]["0"], [0.5, 0.5])
        a, b, native = parts["arm_weapons"]
        self.assertEqual(a["damage"]["base"], 200)
        self.assertEqual(b["damage"]["base"], 100)
        self.assertEqual(b["fire_frames"], 30)
        self.assertEqual(a["damage_min"], 100)
        self.assertEqual(a["slai_balance"]["damage"], [0.5, 0.5])
        self.assertEqual(native["damage"]["base"], 999)

    def test_no_hit_outdoes_the_strongest_native_one_but_price_is_not_capped(self):
        base = {"arm_weapons": [weapon(0, 100, blast=50), weapon(0, 300, blast=100)],
                "shoulder_weapons": []}
        parts = {"arm_weapons": [weapon(0, 100, blast=200, slai_model="A"),
                                 weapon(0, 100, blast=200, slai_model="B"),
                                 weapon(0, 100, blast=20000, price=50000, slai_model="C")],
                 "shoulder_weapons": []}
        balance_weapons(parts, base, SETTINGS)
        nuke = parts["arm_weapons"][2]
        self.assertLessEqual(value(nuke["blast_damage"], 0), 100)
        self.assertLessEqual(value(nuke["blast_damage"], 99), 200)
        self.assertGreater(nuke["price"]["base"], 1000)

    def test_disabled_leaves_the_source_values(self):
        parts = {"arm_weapons": [weapon(0, 400, slai_model="A")], "shoulder_weapons": []}
        before = copy.deepcopy(parts)
        self.assertEqual(balance_weapons(parts, parts, {**SETTINGS, "enabled": False}), {})
        self.assertEqual(parts, before)

    def test_restoring_the_source_makes_the_pass_repeatable(self):
        source = {"index": 0, "kind": 0, "fire_frames": 60, "rounds": curve(1),
                  "damage": curve(400, 400, 0, 100), "blast_damage": curve(0), "damage_min": 200,
                  "price": curve(1000), "weight": curve(100), "source": {"address": "0x1"}}
        stats = {"arm_weapons": [source], "shoulder_weapons": []}
        # As registration copies it: knees 0 to 100 become 0 to 99.
        record = {**copy.deepcopy(source), "name": "S.L.A.I. W", "slai_model": "A"}
        record["damage"]["knee"] = 99
        parts = {"arm_weapons": [record], "shoulder_weapons": []}
        base = {"arm_weapons": [weapon(0, 100), weapon(0, 300)], "shoulder_weapons": []}
        balance_weapons(parts, base, SETTINGS)
        once = copy.deepcopy(parts)
        self.assertEqual(restore_source(parts, stats), 1)
        balance_weapons(parts, base, SETTINGS)
        self.assertEqual(parts, once)

    def test_the_shipped_rules_read(self):
        shipped = rules()
        self.assertTrue(shipped["enabled"])
        self.assertTrue(shipped["groups"]["damage"]["cap_to_strongest"])
        self.assertNotIn("cap_to_strongest", shipped["groups"]["price"])


if __name__ == "__main__":
    unittest.main()
