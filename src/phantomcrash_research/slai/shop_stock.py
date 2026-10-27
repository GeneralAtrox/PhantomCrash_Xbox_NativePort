"""The lineups S.L.A.I.'s shops sell, appended to the pack's garage
(shop_table.py), with their parts' levels held to Phantom Crash's range.

Phantom Crash's parts shops sell nearly every part untuned or 10 levels
either way; S.L.A.I.'s tune theirs and their SVs' up to 60. The cap
(shop_levels.json) is a port balance choice.
"""

import json
from pathlib import Path

CATEGORIES = ("body", "leg", "arm", "shoulder", "option", "paint", "emblem")


def level_cap(path=None):
    """The cap from shop_levels.json; None keeps S.L.A.I.'s own levels."""
    path = path or Path(__file__).with_name("shop_levels.json")
    return json.loads(Path(path).read_text(encoding="utf-8")).get("level_cap")


class Stock:
    """Lineups appended to the garage, each converted once, their parts'
    levels held to the cap (shop_levels.json; None keeps S.L.A.I.'s)."""

    def __init__(self, garage, convert, level_cap=None):
        self.garage, self.convert = garage, convert
        self.level_cap = level_cap
        self.robots, self.modules, self.svs = {}, {}, {}

    def level(self, level):
        if self.level_cap is None:
            return level
        return max(-self.level_cap, min(self.level_cap, level))

    def robot(self, robot_id):
        if robot_id not in self.robots:
            record = self.convert.robot(robot_id)
            for part in record["parts"].values():
                if part is not None:
                    part["level"] = self.level(part["level"])
            self.robots[robot_id] = len(self.garage["robots"])
            self.garage["robots"].append(record)
        return self.robots[robot_id]

    def module_lineup(self, key, category, items):
        if category == CATEGORIES.index("option"):
            return -1  # S.L.A.I.'s options are not the port's modules
        if key not in self.modules:
            lineup = []
            for sid, level in items:
                port = self.convert.item(category, sid)
                entry = {"id": port, "level": self.level(level)}
                # Capping can make two of a lineup's goods the same.
                if port is not None and entry not in lineup:
                    lineup.append(entry)
            self.modules[key] = len(self.garage["module_lineups"])
            self.garage["module_lineups"].append(lineup)
        return self.modules[key]

    def sv_lineup(self, key, robots):
        if key not in self.svs:
            self.svs[key] = len(self.garage["used_sv_lineups"])
            self.garage["used_sv_lineups"].append([self.robot(r) for r in robots])
        return self.svs[key]
