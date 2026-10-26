"""Stage asset descriptions and collision geometry from BG/BGP source files."""

from __future__ import annotations

import json
from pathlib import Path
import struct

import numpy as np

from .model import SCALE
from .pod import read_pod, world_matrices, transform


def _commands(data: bytes) -> list[tuple[str, list[str]]]:
    result = []
    for line in data.decode("cp932", errors="replace").splitlines():
        words = line.split(";", 1)[0].split()
        if words:
            result.append((words[0], words[1:]))
    return result


def _save(path: Path, value) -> None:
    path.parent.mkdir(parents=True, exist_ok=True)
    path.write_text(json.dumps(value, indent=2, allow_nan=False) + "\n", encoding="utf-8")


# A lid must span at least this share of the floor's extent (x by z).
LID_SHARE = 0.5


def move_lids(collision: dict) -> int:
    """Moves an arena lid from the floor to the walls, facing down; returns
    the triangles moved. Port reading: Germany, Hong Kong and Las Vegas each
    have one flat quad over the whole arena above every other standing
    surface (port heights 100, 74.3 and 108.1). As floor it carried robots
    onto it, since the port stands a robot on the floor above its feet; it
    caps the arena, so it is kept as a ceiling. Repeatable."""
    floor = collision["floor"]
    if len(floor) < 2:
        return 0
    triangles = np.array([t["points"] for t in floor], dtype=float)
    heights = triangles[:, :, 1]
    top = heights.max()
    lid = (heights.min(axis=1) > top - 1e-3)
    if lid.all() or not lid.any():
        return 0

    def extent(group):
        flat = group[:, :, [0, 2]].reshape(-1, 2)
        return float(np.prod(flat.max(axis=0) - flat.min(axis=0)))

    if extent(triangles[lid]) < LID_SHARE * extent(triangles[~lid]):
        return 0
    moved = [t for t, on in zip(floor, lid) if on]
    for t in moved:
        t["points"] = [t["points"][0], t["points"][2], t["points"][1]]
    collision["floor"] = [t for t, on in zip(floor, lid) if not on]
    collision["walls"].extend(moved)
    extras = collision.setdefault("extras", {})
    extras["ceiling_triangles"] = extras.get("ceiling_triangles", 0) + len(moved)
    return len(moved)


def _collision(disc, files) -> dict:
    walls, floor = [], []
    for file in files:
        nodes = read_pod(disc.read(file))
        worlds = world_matrices(nodes)
        for node, world in zip(nodes, worlds):
            for material in node.materials:
                batch = material.batch
                points = np.array([transform(world, p) for p in batch.positions]) * SCALE
                for a, b, c in batch.triangles:
                    # The source strips are clockwise after axis conversion;
                    # stored counter-clockwise, as Phantom Crash's collision
                    # is, a standing surface faces up. Stored clockwise, the
                    # port found no ground under any S.L.A.I. floor and held
                    # a robot landing every tick, unable to jump.
                    tri = points[[a, c, b]]
                    normal = np.cross(tri[1] - tri[0], tri[2] - tri[0])
                    length = np.linalg.norm(normal)
                    if length < 1e-6:
                        continue
                    normal = normal / length
                    record = {"points": tri.tolist(), "surface": 0}
                    (floor if normal[1] > 0.5 else walls).append(record)
    collision = {"walls": walls, "floor": floor,
            "extras": {"source_hit_models": [{"source": f.name, "package_offset": f.package} for f in files],
                       "coordinate_conversion": [0.1, -0.1, 0.1],
                       "coordinate_space": "glTF model space; native stage registration pending",
                       "surface_classification": "native adapter from triangle normal; source IDs unavailable"}}
    move_lids(collision)
    return collision


def _lighting(data: bytes) -> dict:
    count32 = struct.unpack_from("<I", data)[0]
    if len(data) == 96 + count32 * 64:
        return {"format": "VLIGHT", "records": [
            {"position_source": list(struct.unpack_from("<4f", data, 96 + i * 64)),
             "rotation_source": list(struct.unpack_from("<4f", data, 112 + i * 64)),
             "colour_source": list(struct.unpack_from("<4f", data, 128 + i * 64)),
             "parameters_source": list(struct.unpack_from("<4f", data, 144 + i * 64))}
            for i in range(count32)],
            "fog_source": list(struct.unpack_from("<4f", data, 16)),
            "fog_colour": list(struct.unpack_from("<4I", data, 32)),
            "sun_angles_source": list(struct.unpack_from("<4f", data, 48)),
            "sun_colour": list(struct.unpack_from("<4f", data, 64)),
            "ambient": list(struct.unpack_from("<4f", data, 80)),
            "status": "header and 64-byte local light records decoded; local light parameters retained"}
    count = struct.unpack_from("<H", data, 4)[0]
    if len(data) != 6 + count * 48:
        return {"status": "record layout unresolved", "size": len(data)}
    records = []
    for i in range(count):
        at = 6 + i * 48
        position, direction = struct.unpack_from("<4f", data, at), struct.unpack_from("<4f", data, at + 16)
        records.append({"position_source": list(position), "direction_source": list(direction),
                        "scalar_at_32": struct.unpack_from("<f", data, at + 32)[0],
                        "words_at_36": list(struct.unpack_from("<3I", data, at + 36))})
    return {"records": records, "status": "source fields retained; light types not assigned"}


def _props(data: bytes) -> dict:
    count = struct.unpack_from("<H", data)[0]
    if len(data) != 2 + count * 48:
        return {"status": "record layout unresolved", "size": len(data)}
    records = []
    for i in range(count):
        at = 2 + i * 48
        values = struct.unpack_from("<8f", data, at)
        records.append({"position_source": list(values[:3]), "value_at_12": values[3],
                        "rotation_source": list(values[4:7]), "value_at_28": values[7],
                        "resource_words": list(struct.unpack_from("<4I", data, at + 32))})
    return {"records": records, "status": "placement fields retained; resource IDs not assigned"}


def export_stages(disc, root: Path, index: dict, model_sources: dict, resources: dict) -> list[dict]:
    result = []
    for name in sorted(n for n in index if n.endswith(".BG")):
        bg_package = {f.name: f for f in disc.packages()[index[name][0].package]}
        commands = _commands(disc.read(index[name][0]))
        parts, hits, missing = [], [], []
        for command, args in commands:
            if command != "LOAD_PARTS_NAME" or not args:
                continue
            part_name = args[0].upper()
            if part_name not in index:
                missing.append(part_name)
                continue
            fields = _commands(disc.read(bg_package.get(part_name, index[part_name][0])))
            models = [v[0].upper() for k, v in fields if k == "POD_NAME" and v]
            hit_models = [v[0].upper() for k, v in fields if k == "HIT_POD_NAME" and v]
            hits.extend(hit_models)
            parts.append({"source": part_name, "directives": fields,
                          "models": [f"../models/{Path(n).stem.lower()}.gltf" for n in models],
                          "hit_models": hit_models,
                          "sky": part_name.endswith("_Z.BGP")})
            missing.extend(n for n in models + hit_models if n not in model_sources)
        stem = Path(name).stem.lower()
        city_folder = name.removesuffix(".BG").removeprefix("DAY_").removeprefix("NIGHT_")
        city_folder = "TOKYO" if city_folder == "TOKYO2" else city_folder
        collision_package = resources.get("Z/COLI/" + city_folder)
        collision_files = ([f for f in disc.packages()[collision_package] if f.name.endswith(".PO2")
                            and "CAM" not in f.name] if collision_package is not None else
                           [index[n][0] for n in dict.fromkeys(hits) if n in index])
        collision = _collision(disc, collision_files)
        _save(root / "stages" / (stem + "_collision.json"), collision)
        lighting, props = [], []
        city = next((v[0].upper() for k, v in commands if k == "MODEL_NAME" and v), "")
        prop_city = city.removeprefix("N_")
        prop_city = {"TOKYO": "SINJYUKU", "ENGLAND": "LONDON"}.get(prop_city, prop_city)
        bg_package = {f.name: f for f in disc.packages()[index[name][0].package]}
        if "VLIGHT.DAT" in bg_package:
            lighting.append({"source": "VLIGHT.DAT", "package_offset": index[name][0].package,
                             **_lighting(disc.read(bg_package["VLIGHT.DAT"]))})
        for asset in index:
            if prop_city and asset in (prop_city + ".MED", prop_city + ".MOD"):
                raw = disc.read(index[asset][0])
                (lighting if asset.endswith(".MED") else props).append(
                    {"source": asset, **(_lighting(raw) if asset.endswith(".MED") else _props(raw))})
        document = {"source": name, "directives": commands, "parts": parts,
                    "collision": stem + "_collision.json", "lighting": lighting,
                    "props": props, "missing": sorted(set(missing)),
                    "native_registration": "pending texture/light/prop resource bindings"}
        texture_folder = next((folder for folder, package in resources.items()
                               if folder.startswith("Z/BG/") and package == index[name][0].package), "")
        document["texture_banks"] = [{"folder": folder, "package_offset": package,
                                       "source_images": [f.name for f in disc.packages()[package]
                                                         if f.name.endswith(".PIG")]}
                                      for folder, package in resources.items()
                                      if texture_folder and folder.startswith(texture_folder + "/PIG")]
        _save(root / "stages" / (stem + ".json"), document)
        result.append({"source": name, "file": f"stages/{stem}.json",
                       "models": sum(len(p["models"]) for p in parts),
                       "collision_walls": len(collision["walls"]), "collision_floor": len(collision["floor"]),
                       "missing": document["missing"]})
    return result
