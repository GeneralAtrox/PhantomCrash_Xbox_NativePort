"""Centre S.L.A.I. parts shown alone in the shops' part preview.

A part shown alone stands at a place picked by its model type
(``data/ui/robot_preview.json`` lone_part). Registration gives an S.L.A.I.
type the place of the Phantom Crash type it was made from, but that place
was set for the Phantom Crash model: an S.L.A.I. model whose geometry lies
elsewhere about its origin is drawn off the window's middle, partly cut by
its frame. Each S.L.A.I. type gets a place of its own instead, the same turn
with the offset moved so that its model's middle lands where the middle of
the Phantom Crash models using that place does.
"""

from __future__ import annotations

import json
import math
from pathlib import Path

import numpy as np

from .assets import save

# lone_part.by_type keys by registration category, and the catalogue table
# whose type gives the model shown alone (an arm or shoulder shows side 0).
KINDS = {"body": ("body", "body_types"), "leg": ("legs", "leg_types"),
         "arm": ("arms", "arm_types"), "shoulder": ("shoulders", "shoulder_types")}


def lone_model(definition: dict) -> int:
    return definition["models"][0] if "models" in definition else definition.get("model", -1)


def model_middle(path: Path) -> np.ndarray | None:
    """The middle of a glTF model's bounding box, in the port's model space
    (glTF with z mirrored, world_model.cpp), nodes placed as cgltf places them."""
    if not path.is_file():
        return None
    gltf = json.loads(path.read_text(encoding="utf-8"))
    nodes = gltf.get("nodes", [])
    parents = {child: index for index, node in enumerate(nodes) for child in node.get("children", [])}

    def local(node: dict) -> np.ndarray:
        if "matrix" in node:
            return np.array(node["matrix"], dtype=float).reshape(4, 4).T
        x, y, z, w = node.get("rotation", [0.0, 0.0, 0.0, 1.0])
        rotation = np.array([[1 - 2 * (y * y + z * z), 2 * (x * y - z * w), 2 * (x * z + y * w)],
                             [2 * (x * y + z * w), 1 - 2 * (x * x + z * z), 2 * (y * z - x * w)],
                             [2 * (x * z - y * w), 2 * (y * z + x * w), 1 - 2 * (x * x + y * y)]])
        out = np.eye(4)
        out[:3, :3] = rotation * np.array(node.get("scale", [1.0, 1.0, 1.0]))
        out[:3, 3] = node.get("translation", [0.0, 0.0, 0.0])
        return out

    def world(index: int) -> np.ndarray:
        matrix = local(nodes[index])
        while index in parents:
            index = parents[index]
            matrix = local(nodes[index]) @ matrix
        return matrix

    corners = []
    for index, node in enumerate(nodes):
        if "mesh" not in node:
            continue
        matrix = world(index)
        for primitive in gltf["meshes"][node["mesh"]]["primitives"]:
            accessor = gltf["accessors"][primitive["attributes"]["POSITION"]]
            low, high = accessor["min"], accessor["max"]
            for corner in np.array(np.meshgrid(*zip(low, high))).T.reshape(-1, 3):
                corners.append((matrix @ np.append(corner, 1.0))[:3])
    if not corners:
        return None
    points = np.array(corners)
    middle = (points.min(axis=0) + points.max(axis=0)) / 2.0
    return middle * np.array([1.0, 1.0, -1.0])


def turned(place: list[float], point: np.ndarray) -> np.ndarray:
    """A point turned as a place turns it (robot_preview.cpp lone_place: x, then
    y, then z, row vectors as math_rotations.cpp), before the offset."""
    x, y, z = point
    for axis, angle in zip("xyz", place[3:6]):
        c, s = math.cos(angle), math.sin(angle)
        if axis == "x":
            y, z = y * c - z * s, y * s + z * c
        elif axis == "y":
            x, z = x * c + z * s, -x * s + z * c
        else:
            x, y = x * c - y * s, x * s + y * c
    return np.array([x, y, z])


def centre_places(preview: dict, catalogue: dict, imported: list[dict], model_roots: list[Path],
                  base_types: dict[str, int]) -> int:
    """Give each imported type still on a Phantom Crash type's place a centred
    place of its own, appended to lone_part.places. base_types counts each
    by_type key's Phantom Crash types. Returns the number of places added."""
    lone = preview["lone_part"]
    places, by_type = lone["places"], lone["by_type"]

    def middle(resource: int) -> np.ndarray | None:
        for root in model_roots:
            found = model_middle(root / f"models/binlink/{resource}.gltf")
            if found is not None:
                return found
        return None

    targets: dict[tuple[str, int], np.ndarray | None] = {}

    def target(key: str, table: str, place_index: int) -> np.ndarray | None:
        # Where the Phantom Crash models standing at this place have their middle.
        if (key, place_index) not in targets:
            points = []
            for type_index in range(min(base_types[key], len(by_type[key]))):
                if by_type[key][type_index] != place_index:
                    continue
                found = middle(lone_model(catalogue[table][type_index]))
                if found is not None:
                    points.append(turned(places[place_index], found) + places[place_index][:3])
            targets[key, place_index] = np.mean(points, axis=0) if points else None
        return targets[key, place_index]

    added = 0
    for entry in imported:
        if entry["category"] not in KINDS:
            continue
        key, table = KINDS[entry["category"]]
        type_index = entry["type"]
        if type_index < base_types[key] or type_index >= len(by_type[key]):
            continue
        place_index = by_type[key][type_index]
        if place_index >= base_types["places"]:
            continue  # already its own
        aim = target(key, table, place_index)
        own = middle(lone_model(catalogue[table][type_index]))
        if aim is None or own is None:
            continue
        place = places[place_index]
        offset = aim - turned(place, own)
        places.append([round(float(v), 4) for v in offset] + list(place[3:6]))
        by_type[key][type_index] = len(places) - 1
        added += 1
    return added


def base_counts(preview: dict) -> dict[str, int]:
    lone = preview["lone_part"]
    return {"places": len(lone["places"]), **{key: len(types) for key, types in lone["by_type"].items()}}


def refit_preview_places(data: Path, assets: Path, pack: Path) -> int:
    """Centre the parts of an installed pack (``place-slai-previews``)."""
    read = lambda path: json.loads(path.read_text(encoding="utf-8"))
    base = base_counts(read(data / "ui/robot_preview.json"))
    preview_file = pack / "data/ui/robot_preview.json"
    preview = read(preview_file)
    catalogue = read(pack / "robots/catalogue.json")
    imported = read(pack / "slai/native_parts.json")["parts"]
    added = centre_places(preview, catalogue, imported, [pack, assets], base)
    if added:
        save(preview_file, preview)
    return added
