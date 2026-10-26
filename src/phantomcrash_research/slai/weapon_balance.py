"""Scale the imported S.L.A.I. weapons to Phantom Crash's (weapon_balance.json).

S.L.A.I.'s tables use their own scale, so its weapons are fitted to the
Phantom Crash weapons of the same table and weapon kind: per field, the ratio
of the two games' median metric at each reference level scales the curves.
Fire rate, rounds and ammo stay S.L.A.I.'s. This is a port adaptation, not
recovered S.L.A.I. behaviour; the source values remain in source_stats.json.
"""

from __future__ import annotations

import copy
import json
import statistics
from pathlib import Path

TABLES = ("arm_weapons", "shoulder_weapons")
LEVEL_MAX = 99


def rules(path: Path | None = None) -> dict:
    path = path or Path(__file__).with_name("weapon_balance.json")
    return json.loads(path.read_text(encoding="utf-8"))


def value(curve: dict, level: int) -> float:
    """The port's LevelCurve::value for a level from 0 to 99, unclamped."""
    base, mid, top, knee = curve["base"], curve["mid"], curve["top"], curve["knee"]
    if knee == 0:
        return base + level / LEVEL_MAX * top
    if knee == LEVEL_MAX:
        return base + level / LEVEL_MAX * mid
    if level < knee:
        return base + level / knee * mid
    return base + mid + (level - knee) / (LEVEL_MAX - knee) * (top - mid)


def _scalar(record: dict, field: str, level: int) -> float:
    entry = record.get(field)
    if isinstance(entry, dict):
        return value(entry, level)
    return float(entry or 0)


def metric(record: dict, group: dict, level: int, settings: dict) -> float:
    """The group's measure fields summed at `level`; per second for a
    weapon of rate_kinds when the group asks."""
    amount = sum(_scalar(record, field, level) for field in group["measure"])
    if group.get("per_second") and record.get("kind") in settings["rate_kinds"]:
        rounds = max(_scalar(record, "rounds", level), 1.0)
        return amount * rounds * 60.0 / max(record.get("fire_frames", 1), 1)
    return amount


def scale_curve(curve: dict, low: float, high: float, digits: int) -> dict:
    """Scale a curve by `low` at level 0 and `high` at 99, blended between."""
    def factor(level):
        return low + (high - low) * level / LEVEL_MAX

    def fit(number):
        return round(number, digits) if digits else int(round(number))

    out = dict(curve)
    knee = curve["knee"]
    base = curve["base"] * low
    out["base"] = fit(base)
    if knee == LEVEL_MAX:
        out["mid"] = fit(value(curve, LEVEL_MAX) * high - base)
        out["top"] = fit(curve["top"] * high)
    else:
        out["top"] = fit(value(curve, LEVEL_MAX) * high - base)
        out["mid"] = fit(value(curve, knee) * factor(knee) - base) if knee else fit(curve["mid"] * low)
    return out


def reference(records: list, settings: dict) -> list:
    """Phantom Crash's named records to measure against: with reference
    "sold", only those priced at one of the levels, the parts its shops sell
    (its enemies' own parts, named 敵, are priced 0)."""
    named = [r for r in records if r.get("name")]
    if settings.get("reference") != "sold":
        return named
    return [r for r in named if any(_scalar(r, "price", level) > 0 for level in settings["levels"])]


def _has(record: dict, group: dict, settings: dict) -> bool:
    return any(metric(record, group, level, settings) > 0 for level in (0, LEVEL_MAX))


def measures(native: list, imported: list, group: dict, settings: dict) -> dict:
    """Per weapon kind: the factors at each level, and the strongest
    Phantom Crash weapon's measure at each level."""
    levels = settings["levels"]
    out = {}
    for kind in sorted({r.get("kind") for r in imported}):
        theirs = [r for r in imported if r.get("kind") == kind and _has(r, group, settings)]
        ours = [r for r in native if r.get("kind") == kind and _has(r, group, settings)]
        if len(ours) < settings["min_samples"]:
            ours = [r for r in native if _has(r, group, settings)]
        if not theirs or not ours:
            continue
        factors, strongest = [], []
        for level in levels:
            mine = [metric(r, group, level, settings) for r in ours]
            source = statistics.median(metric(r, group, level, settings) for r in theirs)
            factors.append(statistics.median(mine) / source if source > 0 else 1.0)
            strongest.append(max(mine))
        out[kind] = (factors, strongest, _strongest_hits(native, kind, group, settings))
    return out


def _strongest_hits(native: list, kind, group: dict, settings: dict) -> dict:
    """Each measure field's largest single value among Phantom Crash's
    weapons of the kind (or the table's), at each level."""
    out = {}
    for field in group["measure"]:
        def has(record):
            return any(_scalar(record, field, level) > 0 for level in (0, LEVEL_MAX))
        ours = [r for r in native if r.get("kind") == kind and has(r)]
        if len(ours) < settings["min_samples"]:
            ours = [r for r in native if has(r)]
        if ours:
            out[field] = [max(_scalar(r, field, level) for r in ours) for level in settings["levels"]]
    return out


def balance_weapons(parts: dict, base_parts: dict, settings: dict | None = None) -> dict:
    """Scale every S.L.A.I. weapon in `parts` (those with slai_model) in place.

    `base_parts` is Phantom Crash's own table, the measure. Returns the
    factors by table, group and weapon kind.
    """
    settings = settings or rules()
    report = {}
    if not settings.get("enabled", True):
        return report
    levels = settings["levels"]
    for table in TABLES:
        native = reference(base_parts[table], settings)
        imported = [r for r in parts[table] if r.get("slai_model")]
        table_report = report.setdefault(table, {})
        for name, group in settings["groups"].items():
            by_kind = measures(native, imported, group, settings)
            table_report[name] = {str(k): [round(f, 4) for f in v[0]] for k, v in by_kind.items()}
            for record in imported:
                found = by_kind.get(record.get("kind"))
                if found is None:
                    continue
                pair = list(found[0])
                if group.get("cap_to_strongest"):
                    for at, level in enumerate(levels):
                        own = metric(record, group, level, settings)
                        if own * pair[at] > found[1][at] > 0:
                            pair[at] = found[1][at] / own
                low, high = pair
                for field in group["measure"]:
                    if not isinstance(record.get(field), dict):
                        continue
                    # No single hit beyond the strongest Phantom Crash one.
                    own = list(pair)
                    for at, level in enumerate(levels):
                        size = _scalar(record, field, level)
                        top = found[2].get(field, [0] * len(levels))[at]
                        if group.get("cap_to_strongest") and size * own[at] > top > 0:
                            own[at] = top / size
                    record[field] = scale_curve(record[field], *own, group.get("round", 0))
                for field in group.get("scalars", []):
                    if field in record:
                        record[field] = int(round(record[field] * low))
                record.setdefault("slai_balance", {})[name] = [round(low, 4), round(high, 4)]
    return report


def restore_source(parts: dict, stats: dict) -> int:
    """Put each imported weapon's curves back to its S.L.A.I. source record,
    as registration copies them, so the pass can run again."""
    from .native_parts import native_curves

    restored = 0
    for table in TABLES:
        sources = {s["source"]["address"]: s for s in stats[table] if not s.get("unused")}
        for record in parts[table]:
            source = sources.get(record.get("source", {}).get("address")) if record.get("slai_model") else None
            if source is None:
                continue
            native_curves(record, copy.deepcopy(source))
            record.pop("slai_balance", None)
            restored += 1
    return restored


def rebalance_pack(assets: Path, pack: Path) -> dict:
    """Re-run the pass on an installed pack's tables/parts.json."""
    from .assets import save

    path = pack / "tables/parts.json"
    parts = json.loads(path.read_text(encoding="utf-8"))
    base_parts = json.loads((assets / "tables/parts.json").read_text(encoding="utf-8"))
    stats = json.loads((pack / "slai/source_stats.json").read_text(encoding="utf-8"))
    restored = restore_source(parts, stats)
    report = balance_weapons(parts, base_parts)
    save(path, parts)
    save(pack / "slai/weapon_balance.json", report)
    return {"restored_weapons": restored, "balance": report}
