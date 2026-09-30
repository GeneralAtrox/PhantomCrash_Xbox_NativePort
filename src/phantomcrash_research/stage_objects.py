"""City scenery instances from the European XBE, separate from the static stage.

0x06f0ab walks 52-byte records at 0x22e638[stage] up to model slot -1.
0x070b80 uses the first slot for the intact mesh, the second for its wreck,
then XYZ position and rotation (X * Y * Z * translation). The collision
template at +44 selects the object kind via 0x22e7e8[stage]; +48 is a gate.
Slots belong to effect-model resources 198-203, with their own texture packs.
Names here are researcher-assigned; resource IDs and transforms are shipped data.
"""

from __future__ import annotations

import struct
from pathlib import Path
from typing import Callable

from . import game_tables, gltf, model, png, texture_pack

OBJECT_RECORDS = 0x22E638
OBJECT_TYPES = 0x22E7E8
OBJECT_RECORD = struct.Struct("<2i6f5i")
CITY_STAGES = 6
EFFECT_MODEL_BASE = 198
DOOR_TYPE = 6
MAX_OBJECTS = 100  # 0x06f0ab's city object limit
OBJECT_COLLISION = 0x22E658
COLLISION_BYTES = 0x1B0  # twelve triangles, 0x070b80
# Hit-handler dispatch at 0x22e8e0; values passed to 0x057760.
BREAK_EFFECTS = {0: 14, 1: 13, 2: 15, 3: 15, 4: 15, 5: 58, 7: 13}
BREAK_SOUND = 0x10  # bank 1; converter WAV filenames use decimal indices
WRECK_HIT_DELAY = 6.0  # type 1 returns to its hit handler after six seconds


def records(image: game_tables._Image, stage: int) -> list[dict]:
    address = image.u32(OBJECT_RECORDS + stage * 4)
    types = image.u32(OBJECT_TYPES + stage * 4)
    result = []
    for index in range(MAX_OBJECTS if stage < CITY_STAGES else 8):
        words = OBJECT_RECORD.unpack_from(image.data, image.offset(address + index * 52, 52))
        if words[0] == -1:
            break
        result.append({
            "index": index, "slot": words[0], "wreck_slot": words[1],
            "position": list(words[2:5]), "rotation": list(words[5:8]),
            "template": words[11], "kind": image.u32(types + words[11] * 4),
            "gate": words[12],
        })
    for entry in result:
        address = image.u32(OBJECT_COLLISION + stage * 4) + entry["template"] * COLLISION_BYTES
        entry["collision"] = [list(struct.unpack_from("<9f", image.data, image.offset(at, 36)))
                              for at in range(address, address + COLLISION_BYTES, 36)]
        kind = entry["kind"]
        entry["break_effect"] = BREAK_EFFECTS.get(kind, -1)
        entry["break_sound"] = (f"sound/sfx/battle/{BREAK_SOUND:02d}.wav"
                                if kind in (0, 1, 2, 3, 4, 7) else "")
        entry["wreck_solid"] = kind in (0, 1)
        entry["wreck_break_effect"] = 15 if kind == 1 else -1
        entry["wreck_hit_delay"] = WRECK_HIT_DELAY if kind == 1 else 0.0
    return result


def convert_objects(executable: bytes, read: Callable[[int], bytes | None], output: Path,
                    write_json: Callable[[Path, object], None]) -> list[dict]:
    image = game_tables._Image(executable)
    resources = {r.id: r for r in game_tables.load(executable).resources}
    result = []
    for stage in range(CITY_STAGES):
        resource = resources[EFFECT_MODEL_BASE + stage]
        data = read(resource.id)
        textures = read(resource.texture_resource)
        if data is None or textures is None:
            raise ValueError(f"Missing scenery model or textures for stage {stage}")
        parsed = model.parse(data)
        uris = []
        for texture in texture_pack.parse(textures):
            fmt = texture.format
            relative = f"textures/stages/{resource.texture_resource}/{texture.index}.png"
            png.write_rgba(output / relative, fmt.width, fmt.height, texture.rgba(textures))
            uris.append(f"../../{relative}")
        objects = records(image, stage)
        for entry in objects:
            entry["mesh"] = entry["slot"] - resource.first_mesh_slot
            entry["wreck_mesh"] = (entry["wreck_slot"] - resource.first_mesh_slot
                                   if entry["wreck_slot"] >= 0 else -1)
            if not 0 <= entry["mesh"] < len(parsed.meshes):
                raise ValueError(f"Invalid scenery mesh on stage {stage}")
            if entry["wreck_mesh"] >= len(parsed.meshes):
                raise ValueError(f"Invalid wreck mesh on stage {stage}")
        # Moving gate leaves use StageDoors, so they must not also be baked into scenery.
        props = [entry for entry in objects if entry["kind"] != DOOR_TYPE]
        props_path = f"models/stages/{stage}_props.gltf"
        doors_path = f"models/stages/{stage}_objects.gltf"
        gltf.write_instances(parsed, output / props_path, uris, props)
        gltf.write(parsed, output / doors_path, uris)
        result.append({"stage": stage, "model_resource": resource.id,
                       "texture_resource": resource.texture_resource,
                       "props_model": props_path, "door_model": doors_path,
                       "objects": objects})
    write_json(output / "tables/stage_objects.json", result)
    return result
