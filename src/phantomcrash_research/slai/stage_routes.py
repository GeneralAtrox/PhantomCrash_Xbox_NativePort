"""S.L.A.I.'s computer-robot routes (``Z/PATH/PATHnn0.PTH``) as port enemy paths.

Each file is a graph like Phantom Crash's ENPT enemy paths (``battle_data``):
a count, a table of record offsets, then per record its kind (u16), link
count (u16), a word, the offset of its curve and the offsets of the records
that may follow it. A curve is a header whose first word counts its knots,
then 3 * (knots - 1) + 1 control points (x, y, z, 1): cubic Bezier pieces.
A link runs from a record's last point to the next record's first, at most
a few units apart at junctions.

Kinds keep Phantom Crash's meanings, which they match: 0 records nothing
links to (where robots come in), 2 pauses (some are a single point), 6
climbs. Only the PS2 data is decoded; S.L.A.I.'s own route follower is not.

The file number is the city's index in the city table times ten
(``ROUTE_FILES``): each file lies on its own stage's ground (89 to 99 per
cent of its curve within three units of the port's navigation grid) and far
less on any other. The loader reaching ``Z/PATH`` (resource folder 4,
directory entry 0x34e210) is not traced yet.
"""

from __future__ import annotations

import json
import math
import struct
from pathlib import Path, PurePosixPath

from .assets import save
from .resources import resource_packages
from .stage_spawns import route_spawns
from .stages import move_lids

# Stage source city -> PATH file (city table order, 0x10 apart in the name).
ROUTE_FILES = {"TOKYO": "PATH000.PTH", "TOKYO2": "PATH000.PTH", "GERMAN": "PATH010.PTH",
               "NEWYORK": "PATH020.PTH", "ENGLAND": "PATH030.PTH", "HONGKONG": "PATH040.PTH",
               "EGYPT": "PATH050.PTH", "LASVEGAS": "PATH060.PTH", "PRACTICE": "PATH070.PTH",
               "MOTION": "PATH080.PTH"}
# Port choice: curves become polylines with a point about this far apart
# (world units), close enough for the drones' point reach to follow a bend.
POINT_SPACING = 4.0


def _cubic(points, t):
    u = 1 - t
    return [u * u * u * points[0][c] + 3 * u * u * t * points[1][c] +
            3 * u * t * t * points[2][c] + t * t * t * points[3][c] for c in range(3)]


def read_route(raw: bytes, scale: float, spacing: float = POINT_SPACING) -> dict:
    """A PTH file as ``{"segments": [{"kind", "points", "next"}]}`` in port space."""
    count = struct.unpack_from("<i", raw, 0)[0]
    if count <= 0 or 4 + 4 * count > len(raw):
        raise ValueError("Unsupported route size")
    offsets = struct.unpack_from(f"<{count}I", raw, 4)
    index_of = {offset: i for i, offset in enumerate(offsets)}
    segments = []
    for offset in offsets:
        kind, links = struct.unpack_from("<2H", raw, offset)
        curve = struct.unpack_from("<I", raw, offset + 8)[0]
        following = struct.unpack_from(f"<{links}I", raw, offset + 12)
        if any(link not in index_of for link in following):
            raise ValueError("Route link to no record")
        knots = struct.unpack_from("<I", raw, curve)[0]
        if not 1 <= knots or curve + 16 + 16 * (3 * (knots - 1) + 1) > len(raw):
            raise ValueError("Unsupported route curve")
        controls = []
        for i in range(3 * (knots - 1) + 1):
            x, y, z, w = struct.unpack_from("<4f", raw, curve + 16 + 16 * i)
            if not all(math.isfinite(v) for v in (x, y, z)) or w != 1:
                raise ValueError("Invalid route point")
            controls.append([x * scale, -y * scale, -z * scale])
        points = [controls[0]]
        for piece in range(knots - 1):
            cubic = controls[3 * piece:3 * piece + 4]
            reach = sum(math.dist(a, b) for a, b in zip(cubic, cubic[1:]))
            steps = max(1, math.ceil(reach / spacing))
            points.extend(_cubic(cubic, step / steps) for step in range(1, steps + 1))
        segments.append({"kind": kind, "points": [[round(v, 4) for v in p] for p in points],
                         "next": [index_of[link] for link in following]})
    return {"segments": segments}


def route_file(source: str) -> str | None:
    city = PurePosixPath(source).stem.upper().removeprefix("DAY_").removeprefix("NIGHT_")
    return ROUTE_FILES.get(city)


def register_stage_routes(disc, data: Path, output: Path) -> dict:
    """Writes each S.L.A.I. stage's route to ``enepath/slai/`` and takes its
    standing starts from the route, so they lie in the arena S.L.A.I.'s
    robots walk; an older pack's arena lids become ceilings (``move_lids``).
    Repeatable on an installed pack (``output``)."""
    from .program import ProgramData
    from .world_units import world_units
    scale = world_units(ProgramData(disc))["world_scale"]
    battle = json.loads((data / "world/training.json").read_text(encoding="utf-8"))["battle"]
    clearance = {"body_height": battle["robot_height"], "body_radius": battle["robot_radius"],
                 "camera_behind": battle["chase_camera"]["behind"],
                 "camera_height": battle["chase_camera"]["height"]}
    stages_file = output / "data/world/stages.json"
    definitions = json.loads(stages_file.read_text(encoding="utf-8"))
    files = {f.name: f for f in disc.packages()[resource_packages(disc)["Z/PATH"]]}
    routed = []
    for name, stage in definitions["stages"].items():
        file = route_file(stage.get("source", "")) if name.startswith("slai_") else None
        if file is None or file not in files:
            continue
        route = read_route(disc.read(files[file]), scale)
        route["source"] = file
        relative = f"enepath/slai/{name}.json"
        save(output / relative, route)
        stage["enemy_path"] = relative
        collision = json.loads((output / stage["collision"]).read_text(encoding="utf-8"))
        if move_lids(collision):  # packs converted before move_lids
            save(output / stage["collision"], collision)
        stage["spawn"], stage["player_spawns"] = route_spawns(
            route, collision["floor"], collision["walls"], **clearance)
        routed.append({"stage": name, "route": file, "segments": len(route["segments"])})
    save(stages_file, definitions)
    return {"routed_stages": routed}
