"""Blueprint maps of S.L.A.I.'s stages for RUMBLING's area screen.

Phantom Crash shows each RUMBLING area as white line art on the area screen's
grid (area_situation_tex, data/ui/rumbling.json areas: map_frames), built up
over four frames, with the opponents marked on it through the area's
robot_map. S.L.A.I. has no such drawings, so they are drawn here from each
stage's collision (collision/slai/<stage>.json): seen from above, the edges
of its floors give the ground's outline and its walls the buildings'
footprints. The frames add detail by height, as the originals do: the
outline, then the buildings rising from the ground, then what stands higher.

The robot_map written beside each drawing is the one it was drawn with
(port/src/rumbling_map.cpp, 0x04438d), so the markers fall where the robots
stand. Gates are the stage's player spawns.
"""

from __future__ import annotations

import json
import math
from collections import defaultdict
from pathlib import Path

import numpy as np
from PIL import Image, ImageDraw

from .assets import save

FRAME = (376, 304)    # canvas units, the original's map frames
MAP_AT = (234, 73)    # where the frames stand (rumbling.json robot_markers origin)
MAP_LAYER = 4
FRAMES = 4
MARGIN = 12.0         # canvas units kept clear round the drawing
DENSITY = 2           # texels per canvas unit, so the lines stay sharp when scaled up
SUPERSAMPLE = 4       # drawn this much finer, then filtered down
LINE_WIDTH = 0.9      # canvas units
ROUND = 0.1           # world units: endpoints this close are one (doubled walls)
MIN_WALL_HEIGHT = 0.3  # world units: lower walls are kerbs and steps
MIN_SEGMENT = 0.25     # world units
MIN_PATCH_AREA = 16.0  # world units squared: smaller floor patches are props' tops
MAX_WALL_SLOPE = 0.35  # a wall whose normal rises more is a slope, outlined by its floors
EXTENT = (1.0, 99.0)   # percentiles of the walls' ends taken as the arena's extent
EXTENT_GROWTH = 0.04   # the extent grown each way by this share of its longer side
GROUND_RISE = 2.0      # world units above the ground still counted as ground
ANGLE_STEP = 1.0       # degrees tried when turning the drawing to fit
ANGLE_TOLERANCE = 0.005  # fits this close to the best prefer the least turn


def _key(point) -> tuple[float, float]:
    return (round(point[0] / ROUND), round(point[2] / ROUND))


def floor_patches(floor: list[dict]) -> list[list[int]]:
    """Floor triangles joined by shared edges, as lists of triangle indices."""
    owners: dict[tuple, list[int]] = defaultdict(list)
    for index, triangle in enumerate(floor):
        points = triangle["points"]
        for i in range(3):
            owners[tuple(sorted((_key(points[i]), _key(points[(i + 1) % 3]))))].append(index)
    parent = list(range(len(floor)))

    def root(i: int) -> int:
        while parent[i] != i:
            parent[i] = parent[parent[i]]
            i = parent[i]
        return i

    for shared in owners.values():
        for other in shared[1:]:
            parent[root(other)] = root(shared[0])
    patches: dict[int, list[int]] = defaultdict(list)
    for index in range(len(floor)):
        patches[root(index)].append(index)
    return list(patches.values())


def _area(points) -> float:
    (ax, _, az), (bx, _, bz), (cx, _, cz) = points
    return abs((bx - ax) * (cz - az) - (cx - ax) * (bz - az)) / 2.0


def segments(collision: dict) -> list[dict]:
    """The drawing's lines in world x/z: each with its ends, its height (the
    top of what it outlines), its base and whether it is a floor's edge."""
    floor = collision.get("floor", [])
    found: dict[tuple, dict] = {}
    wall_ends: list[np.ndarray] = []

    def add(a, b, top: float, base: float, outline: bool) -> None:
        if math.dist((a[0], a[2]), (b[0], b[2])) < MIN_SEGMENT:
            return
        key = tuple(sorted((_key(a), _key(b))))
        if key[0] == key[1]:
            return
        # The same line again (a wall over a floor's edge, a wall's other
        # face) keeps what came first, so the outline is drawn from frame 0.
        if key not in found:
            found[key] = {"ends": [[a[0], a[2]], [b[0], b[2]]], "top": top, "base": base,
                          "outline": outline}

    for patch in floor_patches(floor):
        if len(patch) < 2 or sum(_area(floor[i]["points"]) for i in patch) < MIN_PATCH_AREA:
            continue
        edges: dict[tuple, list] = defaultdict(list)
        for i in patch:
            points = floor[i]["points"]
            for j in range(3):
                a, b = points[j], points[(j + 1) % 3]
                edges[tuple(sorted((_key(a), _key(b))))].append((a, b))
        for pairs in edges.values():
            if len(pairs) == 1:
                a, b = pairs[0]
                height = max(a[1], b[1])
                add(a, b, height, height, True)
    for wall in collision.get("walls", []):
        points = np.array(wall["points"], dtype=float)
        base, top = points[:, 1].min(), points[:, 1].max()
        normal = np.cross(points[1] - points[0], points[2] - points[0])
        length = np.linalg.norm(normal)
        if top - base < MIN_WALL_HEIGHT or length == 0 or abs(normal[1]) / length > MAX_WALL_SLOPE:
            continue
        # An upright wall seen from above is its longest run across the ground.
        i, j = max(((0, 1), (1, 2), (0, 2)),
                   key=lambda e: math.dist(points[e[0]][[0, 2]], points[e[1]][[0, 2]]))
        add(points[i], points[j], top, base, False)
        wall_ends += [points[i][[0, 2]], points[j][[0, 2]]]
    return within_arena(list(found.values()), np.array(wall_ends))


def within_arena(lines: list[dict], ends: np.ndarray) -> list[dict]:
    """Leaves out lines beyond the extent of the walls' ends: the edges of
    ground laid far past the arena, which would shrink the drawing to fit them."""
    if len(ends) == 0:
        return lines
    low, high = np.percentile(ends, EXTENT[0], axis=0), np.percentile(ends, EXTENT[1], axis=0)
    grow = (high - low).max() * EXTENT_GROWTH
    low, high = low - grow, high + grow
    return [line for line in lines
            if all(np.all(low <= end) and np.all(end <= high) for end in np.array(line["ends"]))]


def ground_height(collision: dict) -> float:
    heights = [p[1] for triangle in collision.get("floor", []) for p in triangle["points"]]
    return float(np.percentile(heights, 10)) if heights else 0.0


def frame_of(segment: dict, ground: float, middle: float) -> int:
    """The first frame that draws the line: the ground's outline, the
    buildings rising from the ground, then lower and higher detail."""
    if segment["outline"] and segment["top"] <= ground + GROUND_RISE:
        return 0
    if not segment["outline"] and segment["base"] <= ground + GROUND_RISE:
        return 1
    return 2 if segment["top"] <= middle else 3


def placed(robot_map: dict, x: float, z: float) -> tuple[float, float]:
    """A stage point on the map frame, as rumbling_map.cpp places it."""
    scale, angle = robot_map["scale"], robot_map["angle"]
    pivot, offset = robot_map["pivot"], robot_map["offset"]
    across = x * scale + offset[0] - pivot[0]
    down = -z * scale + offset[1] - pivot[1]
    c, s = math.cos(angle), math.sin(angle)
    return (pivot[0] + across * c + down * s, pivot[1] - across * s + down * c)


def fit(points: np.ndarray) -> dict:
    """The robot_map that turns and scales the points to fill the frame,
    turned as little as a near-best fit allows."""
    tried = []
    for step in range(int(180 / ANGLE_STEP)):
        angle = math.radians(-90.0 + step * ANGLE_STEP)
        c, s = math.cos(angle), math.sin(angle)
        u = points[:, 0] * c - points[:, 1] * s
        v = -points[:, 0] * s - points[:, 1] * c
        width, height = np.ptp(u), np.ptp(v)
        scale = min((FRAME[0] - 2 * MARGIN) / max(width, 1e-6),
                    (FRAME[1] - 2 * MARGIN) / max(height, 1e-6))
        tried.append((scale, angle, (u.min() + u.max()) / 2, (v.min() + v.max()) / 2))
    best = max(scale for scale, *_ in tried)
    scale, angle, u_middle, v_middle = min(
        (entry for entry in tried if entry[0] >= best * (1 - ANGLE_TOLERANCE)),
        key=lambda entry: abs(entry[1]))
    c, s = math.cos(angle), math.sin(angle)
    shift_u = FRAME[0] / 2 - scale * u_middle
    shift_v = FRAME[1] / 2 - scale * v_middle
    return {"offset": [round(shift_u * c - shift_v * s, 4), round(shift_u * s + shift_v * c, 4)],
            "pivot": [0, 0], "angle": round(angle, 6), "scale": round(scale, 6)}


def draw(lines: list[dict], robot_map: dict, ground: float) -> tuple[Image.Image, list[int]]:
    """The four frames side by side in a 2x2 sheet, and the lines each adds."""
    tops = [line["top"] for line in lines if frame_of(line, ground, math.inf) >= 2]
    middle = float(np.median(tops)) if tops else ground
    width, height = FRAME[0] * DENSITY, FRAME[1] * DENSITY
    fine = DENSITY * SUPERSAMPLE
    frames = [Image.new("L", (width * SUPERSAMPLE, height * SUPERSAMPLE), 0) for _ in range(FRAMES)]
    pens = [ImageDraw.Draw(frame) for frame in frames]
    counts = [0] * FRAMES
    for line in lines:
        first = frame_of(line, ground, middle)
        counts[first] += 1
        ends = [tuple(coordinate * fine for coordinate in placed(robot_map, x, z))
                for x, z in line["ends"]]
        for pen in pens[first:]:
            pen.line(ends, fill=255, width=max(1, round(LINE_WIDTH * fine)))
    sheet = Image.new("RGBA", (width * 2, height * 2), (255, 255, 255, 0))
    for index, frame in enumerate(frames):
        ink = Image.new("RGBA", (width, height), (255, 255, 255, 0))
        ink.putalpha(frame.resize((width, height), Image.LANCZOS))
        sheet.paste(ink, ((index % 2) * width, (index // 2) * height))
    return sheet, counts


def area_map(collision: dict, spawns: list, image: str, name: str) -> tuple[Image.Image, dict]:
    """A stage's drawing and its area entry (rumbling.json areas' map_frames,
    robot_map, gates and marks)."""
    lines = segments(collision)
    ends = np.array([end for line in lines for end in line["ends"]], dtype=float)
    robot_map = fit(ends)
    sheet, counts = draw(lines, robot_map, ground_height(collision))
    width, height = FRAME[0] * DENSITY, FRAME[1] * DENSITY
    frames = [{"id": f"{name}_map_{index}", "image": image,
               "rect": [(index % 2) * width, (index // 2) * height,
                        (index % 2 + 1) * width, (index // 2 + 1) * height],
               "size": list(FRAME), "position": list(MAP_AT), "layer": MAP_LAYER}
              for index in range(FRAMES)]
    gates = []
    for spawn in spawns:
        u, v = placed(robot_map, spawn[0], spawn[2])
        gates.append([round(float(MAP_AT[0] + u), 1), round(float(MAP_AT[1] + v), 1)])
    return sheet, {"map_frames": frames, "robot_map": robot_map, "gates": gates, "marks": [],
                   "lines_by_frame": counts}


def rumble_stages(data: Path) -> list[str]:
    """The stages S.L.A.I.'s rumbles are fought on (world/slai_rumble.json cities)."""
    world = json.loads((data / "world/slai_rumble.json").read_text(encoding="utf-8"))
    names = [city[key] for city in world["cities"] for key in ("day_stage", "night_stage")]
    return sorted(set(names))


def register_area_maps(data: Path, pack: Path) -> dict:
    """Draw the map of each installed stage S.L.A.I.'s rumbles use into the
    pack (textures/slai/maps, data/ui/slai_area_maps.json)."""
    stages = json.loads((pack / "data/world/stages.json").read_text(encoding="utf-8"))["stages"]
    areas = {}
    for name in rumble_stages(data):
        stage = stages.get(name, {})
        collision_file = pack / stage.get("collision", "")
        if not stage or not collision_file.is_file():
            continue
        collision = json.loads(collision_file.read_text(encoding="utf-8"))
        image = f"textures/slai/maps/{name}.png"
        spawns = stage.get("player_spawns") or [stage.get("spawn", [0.0, 0.0, 0.0])]
        sheet, areas[name] = area_map(collision, spawns, image, name)
        (pack / image).parent.mkdir(parents=True, exist_ok=True)
        sheet.save(pack / image, optimize=True)
    save(pack / "data/ui/slai_area_maps.json", {
        "notes": [
            "Blueprint maps of S.L.A.I.'s stages for RUMBLING's area screen, drawn from their "
            "collision by slai/area_maps.py (a port addition: S.L.A.I. has none). By stage: "
            "map_frames build the drawing up as rumbling.json's areas do, robot_map is the "
            "placement it was drawn with, gates are the stage's player spawns on the screen, "
            "marks none (S.L.A.I.'s counterpart of the H marks is untraced). lines_by_frame "
            "counts the lines each frame adds.",
        ],
        "areas": areas})
    return {name: entry["lines_by_frame"] for name, entry in areas.items()}
