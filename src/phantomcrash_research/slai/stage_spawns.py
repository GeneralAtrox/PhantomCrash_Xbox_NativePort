"""Native standing starts sampled from imported collision with body/camera clearance."""

import numpy as np


def _enclosures(triangles, horizontal, body_height):
    """Separate broad arena ceilings from individual building roofs."""
    face_heights = triangles[:, :, 1].mean(axis=1)
    typical = np.median(face_heights[horizontal])
    standing = triangles[horizontal & (face_heights <= typical)]
    support = standing[:, :, [0, 2]].reshape(-1, 2)
    support_area = np.prod(np.ptp(support, axis=0))
    owners = {}
    for index, face in enumerate(triangles):
        for vertex in face:
            owners.setdefault(tuple(np.round(vertex, 5)), []).append(index)
    remaining = set(range(len(triangles)))
    enclosure = np.zeros(len(triangles), dtype=bool)
    while remaining:
        component, pending = [], [remaining.pop()]
        while pending:
            index = pending.pop()
            component.append(index)
            for vertex in triangles[index]:
                for neighbor in owners[tuple(np.round(vertex, 5))]:
                    if neighbor in remaining:
                        remaining.remove(neighbor)
                        pending.append(neighbor)
        points = triangles[component].reshape(-1, 3)
        if (points[:, 1].min() > typical+body_height and
                np.prod(np.ptp(points[:, [0, 2]], axis=0)) >= support_area*.7):
            enclosure[component] = True
    return enclosure


def _walls(floor, walls):
    """The walls, less the ceilings that only turn a floor's underside
    (two_sided): this clearance test has no facing, so a robot's own floor
    would block it."""
    floors = {frozenset(tuple(np.round(p, 4)) for p in t["points"]) for t in floor}
    kept = [t["points"] for t in walls
            if frozenset(tuple(np.round(p, 4)) for p in t["points"]) not in floors]
    return np.array(kept, dtype=float).reshape(-1, 3, 3)


def _wall_distance(point, triangles):
    if not len(triangles):
        return float("inf")
    a, b, c = triangles[:, 0], triangles[:, 1], triangles[:, 2]
    ab, ac = b-a, c-a
    normal = np.cross(ab, ac)
    norm2 = (normal*normal).sum(axis=1)
    signed = ((point-a)*normal).sum(axis=1) / np.maximum(norm2, 1e-20)
    projected = point - signed[:, None]*normal
    ap = projected-a
    aa, bb, cc = (ab*ab).sum(axis=1), (ab*ac).sum(axis=1), (ac*ac).sum(axis=1)
    denom = np.maximum(aa*cc-bb*bb, 1e-20)
    da, dc = (ap*ab).sum(axis=1), (ap*ac).sum(axis=1)
    u, v = (cc*da-bb*dc)/denom, (aa*dc-bb*da)/denom
    inside = (u >= 0) & (v >= 0) & (u+v <= 1) & (norm2 > 1e-20)
    distance = np.where(inside, signed*signed*norm2, np.inf)
    for start, end in ((a, b), (b, c), (c, a)):
        edge = end-start
        t = np.clip(((point-start)*edge).sum(axis=1) /
                    np.maximum((edge*edge).sum(axis=1), 1e-20), 0, 1)
        delta = point-start-t[:, None]*edge
        distance = np.minimum(distance, (delta*delta).sum(axis=1))
    return float(np.sqrt(distance.min()))


def _clear(point, wall_triangles, body_height, body_radius, camera_behind, camera_height):
    """A body standing at `point` clears the walls, and so does a chase
    camera behind it whichever way it faces."""
    if any(_wall_distance(point+[0, h, 0], wall_triangles) < body_radius
           for h in (body_radius, body_height-body_radius)):
        return False
    eyes = point+[0, camera_height, 0]
    for direction in ([1, 0, 0], [-1, 0, 0], [0, 0, 1], [0, 0, -1]):
        for fraction in (1/3, 2/3, 1):
            camera = eyes+np.array(direction)*camera_behind*fraction
            if _wall_distance(camera, wall_triangles) < body_radius:
                return False
    return True


def _floor_at(point, triangles, rise):
    """The floor under `point` nearest its height, within `rise`, or None."""
    xz = triangles[:, :, [0, 2]]
    edges = (xz[:, 1:]-xz[:, :1]).transpose(0, 2, 1)
    valid = abs(np.linalg.det(edges)) > 1e-8
    uv = np.einsum("nij,nj->ni", np.linalg.inv(edges[valid]), np.array(point)[[0, 2]]-xz[valid, 0])
    covered = (uv.min(axis=1) >= -1e-5) & (uv.sum(axis=1) <= 1.00001)
    corners = triangles[valid]
    heights = corners[:, 0, 1] + (uv*(corners[:, 1:, 1]-corners[:, :1, 1])).sum(axis=1)
    heights = heights[covered]
    if not len(heights):
        return None
    nearest = heights[np.argmin(abs(heights-point[1]))]
    return float(nearest) if abs(nearest-point[1]) <= rise else None


def route_spawns(route, floor, walls=(), *, body_height=4.4, body_radius=1.5,
                 camera_behind=6.15, camera_height=2.0, rise=2.0, count=4):
    """Four starts on the stage's own route, so they lie in the arena its
    robots walk, as far from where they come in (kind 0) and from each other
    as the clearance tests allow; the first is the player's. Still adapter
    starts: S.L.A.I.'s own start table is not recovered."""
    triangles = np.array([t["points"] for t in floor])
    wall_triangles = _walls(floor, walls)
    points = {tuple(p) for s in route["segments"] for p in s["points"]}
    remaining = np.array(sorted(points))
    entries = np.array([s["points"][0] for s in route["segments"] if s["kind"] == 0]).reshape(-1, 3)
    anchors = entries if len(entries) else remaining.mean(axis=0, keepdims=True)
    starts = []
    while len(starts) < count and len(remaining):
        others = np.array(starts) if starts else anchors
        flat = remaining[:, None, [0, 2]] - others[None, :, [0, 2]]
        pick = int(np.argmax(np.sqrt((flat*flat).sum(axis=2)).min(axis=1)))
        point = remaining[pick].copy()
        remaining = np.delete(remaining, pick, axis=0)
        ground = _floor_at(point, triangles, rise)
        if ground is None:
            continue
        point[1] = ground+.05
        if not len(wall_triangles) or _clear(point, wall_triangles, body_height, body_radius,
                                              camera_behind, camera_height):
            starts.append(point)
    if not starts:
        raise ValueError("No route point has native body/camera clearance")
    starts = [[round(float(v), 4) for v in p] for p in starts]
    return starts[0], starts


def ground_spawns(floor, walls=(), *, body_height=4.4, body_radius=1.5,
                  camera_behind=6.15, camera_height=2.0):
    """Prefer open standing surfaces over enclosed rooms and narrow corners.

    Source hit meshes include upward-facing arena enclosures. Keep the lowest
    standing layer, but reject local roofs and test body/camera clearance against
    walls. These are native adapter starts, not recovered original spawn points.
    """
    triangles = np.array([t["points"] for t in floor])
    wall_triangles = _walls(floor, walls)
    normals = np.cross(triangles[:, 1]-triangles[:, 0], triangles[:, 2]-triangles[:, 0])
    area = np.linalg.norm(normals, axis=1)
    horizontal = abs(normals[:, 1]) / np.maximum(area, 1e-8) > .95
    candidates = np.where(horizontal, area, 0) if horizontal.any() else area
    weights = ([.6, .2, .2], [.2, .6, .2], [.2, .2, .6], [1/3]*3)
    xz = triangles[:, :, [0, 2]]
    edges = (xz[:, 1:]-xz[:, :1]).transpose(0, 2, 1)
    valid = abs(np.linalg.det(edges)) > 1e-8
    inverse = np.linalg.inv(edges[valid])
    enclosure = (_enclosures(triangles, horizontal, body_height)
                 if horizontal.any() else np.zeros(len(triangles), dtype=bool))
    accepted = []
    for index in np.argsort(candidates)[::-1][:64]:
        if candidates[index] <= 0:
            continue
        for w in weights:
            point = np.array(w) @ triangles[index]
            uv = np.einsum("nij,nj->ni", inverse, point[[0, 2]]-xz[valid, 0])
            covered = (uv.min(axis=1) >= -1e-5) & (uv.sum(axis=1) <= 1.00001)
            heights = triangles[valid, 0, 1] + (uv*(triangles[valid, 1:, 1]-triangles[valid, :1, 1])).sum(axis=1)
            ground = heights[covered].min()
            owners = covered & (abs(heights-ground) < 1e-4)
            if enclosure[valid][owners].all():
                continue
            point[1] = ground+.05
            if not len(walls):
                accepted.append(point.tolist())
                if len(accepted) == 4:
                    return accepted[-1], accepted
                continue
            roofs = covered & (heights > ground+.1) & ~enclosure[valid]
            if roofs.any():
                continue
            if _clear(point, wall_triangles, body_height, body_radius, camera_behind, camera_height):
                accepted.append(point.tolist())
    if not accepted:
        raise ValueError("No source standing surface has native body/camera clearance")
    # Pick four separated starts from the clearance-tested candidates.
    starts = [accepted[0]]
    while len(starts) < 4:
        point = max(accepted, key=lambda p: min(np.linalg.norm(np.array(p)-q) for q in starts))
        starts.append(point)
    return starts[-1], starts
