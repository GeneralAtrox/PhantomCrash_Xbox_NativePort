"""Refit an installed pack's S.L.A.I. prices to S.L.A.I.'s own curve shape
(source_shape in armour_balance.json and weapon_balance.json).

A pack balanced before source_shape scaled each price curve by its level-0
factor at level 0 blended to its level-99 factor at 99 (scale_curve); each
part records the pair in slai_balance. This refits the curve as if the
level-0 factor had scaled it throughout, without the source records.
"""

from __future__ import annotations

import json
from pathlib import Path

from .weapon_balance import LEVEL_MAX

TABLES = ("bodies", "legs", "arm_weapons", "shoulder_weapons")


def source_shape(curve: dict, low: float, high: float) -> dict:
    """scale_curve(source, low, high) refitted to scale_curve(source, low, low)."""
    if high <= 0 or low == high:
        return dict(curve)
    out = dict(curve)
    base, knee = curve["base"], curve["knee"]
    if knee == LEVEL_MAX:
        out["mid"] = int(round((curve["mid"] + base) / high * low - base))
        out["top"] = int(round(curve["top"] / high * low))
        return out
    out["top"] = int(round((curve["top"] + base) / high * low - base))
    if knee:
        at_knee = low + (high - low) * knee / LEVEL_MAX
        out["mid"] = int(round((curve["mid"] + base) / at_knee * low - base))
    return out


def keep_source_shape(parts: dict, field: str = "price") -> int:
    """Refit every imported part's `field` whose recorded factors differ;
    returns how many changed."""
    changed = 0
    for table in TABLES:
        for record in parts.get(table, []):
            pair = record.get("slai_balance", {}).get(field)
            if not isinstance(record.get(field), dict) or not pair or pair[0] == pair[-1]:
                continue
            record[field] = source_shape(record[field], pair[0], pair[-1])
            record["slai_balance"][field] = [pair[0]] * len(pair)
            changed += 1
    return changed


def _level_zero(factors):
    """Factors by level (or by weapon kind) as the level-0 factor at each."""
    if isinstance(factors, dict):
        return {key: _level_zero(value) for key, value in factors.items()}
    return [factors[0]] * len(factors)


def _flatten_report(report: dict, field: str) -> dict:
    """A balance report (by table, then field or group) with `field` refitted."""
    return {table: {name: _level_zero(value) if name == field else value
                    for name, value in fields.items()}
            for table, fields in report.items()}


def reshape_pack(pack: Path) -> dict:
    """Refit an installed pack's tables/parts.json and its balance reports."""
    from .assets import save

    path = pack / "tables/parts.json"
    parts = json.loads(path.read_text(encoding="utf-8"))
    changed = keep_source_shape(parts)
    save(path, parts)
    for name in ("slai/armour_balance.json", "slai/weapon_balance.json"):
        report = pack / name
        if report.exists():
            save(report, _flatten_report(json.loads(report.read_text(encoding="utf-8")), "price"))
    return {"refitted": changed}
