"""Convert the city battle stages: Shinjuku, Shibuya and Umihotaru, by day
and by night (stages 0-5 of the stage table at ``0x2212ac``).

Output, under the converted assets:

- ``models/stages/<resource>.gltf``: the stage model (every mesh), with
  ``textures/stages/<pack>/<index>.png``, its texture pack. Stage ``s``'s
  model is resource ``0x2212ac[s]`` (186-191); its pack is resource
  1558 + s. That pairing is a researcher reading: the training ground's is
  the same (model 192, pack 1564), and each pack holds exactly the
  textures its model's batches name.
- ``enepath/<pack name>_enepath.json``: the area's enemy path (resources
  0x697-0x699, one a pack, :mod:`battle_data`), the graph the RUMBLING
  opponents walk.
- ``tables/stages.json``: per stage its model, texture pack, sky meshes and
  enemy path. The sky meshes are the object list's (``0x2212ec``, 16-byte
  entries ``{slot, kind, ...}`` up to slot -1) entries of kind 1, drawn in
  view layer 1 around the camera; slot ``first + n`` is mesh ``n``.
"""

from __future__ import annotations

import struct
from pathlib import Path
from typing import Callable

from . import battle_data, game_tables, gltf, model, png, texture_pack

STAGE_MODELS = 0x2212AC  # u32 model resource per stage
STAGE_OBJECTS = 0x2212EC  # u32 object list per stage
CITY_STAGES = 6  # Shinjuku, Shibuya, Umihotaru by day and night
STAGE_TEXTURE_BASE = 1558  # stage s's texture pack: 1558 + s
OBJECT_ENTRY = struct.Struct("<4I")
OBJECT_END = 0xFFFFFFFF
OBJECT_SKY = 1  # the entry's kind: view layer 1, the sky
# The areas' enemy paths (resources 0x697-0x699): area a's by day and night.
AREA_PATHS = (0x697, 0x698, 0x699)


def _objects(image: game_tables._Image, stage: int) -> list[tuple[int, int]]:
    (address,) = struct.unpack_from("<I", image.data, image.offset(STAGE_OBJECTS + stage * 4, 4))
    out = []
    while True:
        slot, kind, _, _ = OBJECT_ENTRY.unpack_from(image.data, image.offset(address + len(out) * 16, 16))
        if slot == OBJECT_END:
            return out
        out.append((slot, kind))


def convert_stages(
    executable: bytes,
    read: Callable[[int], bytes | None],
    pack_name: Callable[[int], str | None],
    output: Path,
    write_json: Callable[[Path, object], None],
    report: Callable[[str], None],
) -> list[dict]:
    """``read(resource)`` gives a binlink resource's bytes; ``pack_name`` the
    stem of the pack it is in (``b00_shinjuku``)."""
    image = game_tables._Image(executable)
    models = struct.unpack_from(f"<{CITY_STAGES}I", image.data, image.offset(STAGE_MODELS, 4 * CITY_STAGES))
    paths: dict[int, str] = {}
    for resource in AREA_PATHS:
        data = read(resource)
        name = pack_name(resource)
        if data is None or name is None:
            continue
        paths[resource] = f"enepath/{name}_enepath.json"
        write_json(output / paths[resource], battle_data.enemy_path(data))
    records = []
    for stage, model_id in enumerate(models):
        data = read(model_id)
        pack_id = STAGE_TEXTURE_BASE + stage
        textures = read(pack_id)
        if data is None or textures is None:
            continue
        count = 0
        for texture in texture_pack.parse(textures):
            fmt = texture.format
            png.write_rgba(output / "textures" / "stages" / str(pack_id) / f"{texture.index}.png",
                           fmt.width, fmt.height, texture.rgba(textures))
            count = max(count, texture.index + 1)
        uris = [f"../../textures/stages/{pack_id}/{index}.png" for index in range(count)]
        summary = gltf.write(model.parse(data), output / "models" / "stages" / f"{model_id}.gltf", uris)
        objects = _objects(image, stage)
        first = min(slot for slot, _ in objects) if objects else 0
        sky = [f"mesh{slot - first}_node0" for slot, kind in objects if kind == OBJECT_SKY]
        path = paths.get(AREA_PATHS[stage // 2])
        records.append({
            "stage": stage,
            "model": f"models/stages/{model_id}.gltf",
            "textures": pack_id,
            "sky_nodes": sky,
            "enemy_path": path,
            **summary,
        })
    write_json(output / "tables" / "stages.json", records)
    report(f"{len(records):>4} city stages, {len(paths)} area paths")
    return records
