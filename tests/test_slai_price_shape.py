"""S.L.A.I.'s prices on S.L.A.I.'s own curve shape (source_shape), on
hand-made curves: the balance passes, and the refit of an older pack."""

import unittest

from phantomcrash_research.slai.armour_balance import balance_armour, rules as armour_rules
from phantomcrash_research.slai.price_shape import keep_source_shape, source_shape
from phantomcrash_research.slai.weapon_balance import rules as weapon_rules, scale_curve, value


def curve(base, mid, top, knee):
    return {"base": base, "mid": mid, "top": top, "knee": knee, "low_scale": 1}


def body(price, **extra):
    flat = {"base": 1000, "mid": 0, "top": 0, "knee": 0, "low_scale": -1}
    return {"name": "B", "armour": flat, "weight": flat, "price": price, **extra}


class PriceShapeTests(unittest.TestCase):
    def test_the_refit_matches_one_factor_throughout(self):
        for knee in (0, 40, 99):
            source = curve(20000, 400000, 885000, knee)
            blended = scale_curve(source, 0.1, 0.5, 0)
            want = scale_curve(source, 0.1, 0.1, 0)
            got = source_shape(blended, 0.1, 0.5)
            for level in range(100):
                self.assertAlmostEqual(value(got, level), value(want, level), delta=2)
            # Untuned, it costs what it did; tuned, a fifth of the blend's at 99.
            self.assertEqual(value(got, 0), value(blended, 0))
            self.assertLess(value(got, 99), value(blended, 99) / 4)

    def test_balance_keeps_the_source_shape_for_the_fields_named(self):
        base = {"bodies": [body(curve(2000, 80000, 480000, 40))]}
        parts = {"bodies": [body(curve(20000, 400000, 885000, 40), slai_model="A")]}
        settings = {"enabled": True, "levels": [0, 99], "source_shape": ["price"],
                    "tables": {"bodies": ["price"]}}
        report = balance_armour(parts, base, settings)
        self.assertEqual(report, {"bodies": {"price": [0.1, 0.1]}})
        price = parts["bodies"][0]["price"]
        self.assertEqual(value(price, 0), 2000)
        self.assertAlmostEqual(value(price, 99), 90500, delta=1)

    def test_an_older_pack_is_refitted_once(self):
        source = curve(20000, 400000, 885000, 40)
        parts = {"bodies": [body(scale_curve(source, 0.1, 0.5, 0), slai_model="A",
                                 slai_balance={"price": [0.1, 0.5]}),
                            body(curve(2000, 80000, 480000, 40))]}
        self.assertEqual(keep_source_shape(parts), 1)
        self.assertEqual(parts["bodies"][0]["slai_balance"]["price"], [0.1, 0.1])
        self.assertEqual(parts["bodies"][1]["price"], curve(2000, 80000, 480000, 40))
        self.assertEqual(keep_source_shape(parts), 0)

    def test_the_shipped_rules_keep_the_price_shape(self):
        self.assertEqual(armour_rules()["source_shape"], ["price"])
        self.assertTrue(weapon_rules()["groups"]["price"]["source_shape"])


if __name__ == "__main__":
    unittest.main()
