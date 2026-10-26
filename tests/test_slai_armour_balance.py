"""The S.L.A.I. armour balance pass, on hand-made tables."""

import unittest

from phantomcrash_research.slai.armour_balance import balance_armour, rules
from phantomcrash_research.slai.weapon_balance import value


def part(armour_low, armour_high, **extra):
    armour = {"base": armour_low, "mid": 0, "top": armour_high - armour_low, "knee": 0,
              "low_scale": -1}
    return {"name": "P", "armour": armour, "weight": {"base": 100, "mid": 0, "top": 0, "knee": 0},
            **extra}


SETTINGS = {"enabled": True, "levels": [0, 99], "tables": {"bodies": ["armour"]}}


class ArmourBalanceTests(unittest.TestCase):
    def test_the_medians_meet_and_each_part_keeps_its_rank(self):
        base = {"bodies": [part(10000, 30000), part(16000, 40000), part(20000, 50000)]}
        parts = {"bodies": [part(1000, 2000), part(30000, 40000, slai_model="A"),
                            part(20000, 50000, slai_model="B"),
                            part(40000, 60000, slai_model="C")]}
        report = balance_armour(parts, base, SETTINGS)
        # Medians: 16000 over 30000 at level 0, 40000 over 50000 at 99.
        self.assertEqual(report, {"bodies": {"armour": [0.5333, 0.8]}})
        a, b, c = parts["bodies"][1:]
        self.assertAlmostEqual(value(a["armour"], 0), 16000, delta=1)
        self.assertAlmostEqual(value(a["armour"], 99), 32000, delta=1)
        self.assertLess(value(b["armour"], 0), value(a["armour"], 0))
        self.assertLess(value(a["armour"], 0), value(c["armour"], 0))
        self.assertEqual(a["slai_balance"]["armour"], [0.5333, 0.8])
        # Phantom Crash's own parts, and other curves, stay as they were.
        self.assertEqual(parts["bodies"][0]["armour"]["base"], 1000)
        self.assertEqual(a["weight"]["base"], 100)

    def test_disabled_keeps_the_source_armour(self):
        parts = {"bodies": [part(30000, 40000, slai_model="A")]}
        self.assertEqual(balance_armour(parts, {"bodies": [part(1, 2)]},
                                        {**SETTINGS, "enabled": False}), {})
        self.assertEqual(parts["bodies"][0]["armour"]["base"], 30000)

    def test_sold_parts_are_the_reference_and_weight_scales_too(self):
        flat = {"base": 0, "mid": 0, "top": 0, "knee": 0}
        sold = {"base": 1000, "mid": 0, "top": 0, "knee": 0}
        # An enemy body, priced 0, would pull the median down to 10000.
        base = {"bodies": [part(10000, 10000, price=flat), part(10000, 10000, price=flat),
                           part(20000, 20000, price=sold)]}
        parts = {"bodies": [part(40000, 40000, slai_model="A")]}
        parts["bodies"][0]["weight"] = {"base": 200, "mid": 0, "top": 0, "knee": 0}
        settings = {**SETTINGS, "reference": "sold", "tables": {"bodies": ["armour", "weight"]}}
        report = balance_armour(parts, base, settings)
        self.assertEqual(report, {"bodies": {"armour": [0.5, 0.5], "weight": [0.5, 0.5]}})
        self.assertEqual(value(parts["bodies"][0]["weight"], 0), 100)

    def test_the_shipped_rules_cover_bodies_and_legs(self):
        self.assertEqual(rules()["reference"], "sold")
        self.assertEqual(rules()["tables"], {"bodies": ["armour", "weight", "price"],
                                             "legs": ["armour", "weight", "capacity", "price"]})


if __name__ == "__main__":
    unittest.main()
