"""Scale the imported S.L.A.I. bodies' and legs' armour, weight, capacity and
price to Phantom Crash's (armour_balance.json).

Per table and field, the ratio of the two games' median at each reference
level scales every imported part's curve, as weapon_balance.py does for the
weapons. This is a port adaptation, not recovered S.L.A.I. behaviour; the
source values remain in source_stats.json.
"""

from __future__ import annotations

import json
import statistics
from pathlib import Path

from .weapon_balance import reference, scale_curve, value


def rules(path: Path | None = None) -> dict:
    path = path or Path(__file__).with_name("armour_balance.json")
    return json.loads(path.read_text(encoding="utf-8"))


def _curves(records: list, field: str) -> list:
    return [r for r in records if isinstance(r.get(field), dict)]


def factors(native: list, imported: list, field: str, levels: list) -> list | None:
    """Phantom Crash's median over S.L.A.I.'s at each level; None without
    parts on either side."""
    ours, theirs = _curves(native, field), _curves(imported, field)
    if not ours or not theirs:
        return None
    out = []
    for level in levels:
        source = statistics.median(value(r[field], level) for r in theirs)
        mine = statistics.median(value(r[field], level) for r in ours)
        out.append(mine / source if source > 0 else 1.0)
    return out


def balance_armour(parts: dict, base_parts: dict, settings: dict | None = None) -> dict:
    """Scale every S.L.A.I. body and legs in `parts` (those with slai_model)
    in place against Phantom Crash's own `base_parts`. Returns the factors by
    table and field."""
    settings = settings or rules()
    report = {}
    if not settings.get("enabled", True):
        return report
    levels = settings["levels"]
    for table, fields in settings["tables"].items():
        native = reference(base_parts[table], settings)
        imported = [r for r in parts[table] if r.get("slai_model")]
        for field in fields:
            pair = factors(native, imported, field, levels)
            if pair is None:
                continue
            if field in settings.get("source_shape", []):
                pair = [pair[0]] * len(pair)
            report.setdefault(table, {})[field] = [round(f, 4) for f in pair]
            for record in _curves(imported, field):
                record[field] = scale_curve(record[field], *pair, 0)
                record.setdefault("slai_balance", {})[field] = [round(f, 4) for f in pair]
    return report
