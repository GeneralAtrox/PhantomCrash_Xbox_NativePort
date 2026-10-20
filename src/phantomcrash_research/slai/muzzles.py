"""Recovered S.L.A.I. gameplay muzzles and muzzle flashes (battledvd.bin).

Gameplay muzzles: the weapon setup at 0x504b30 walks a part descriptor's 51
node callbacks and appends every node whose callback is 0x4e6db0 to the
weapon's muzzle list (+0x116, count +0x114). The muzzle-frame getter 0x5056e0
keeps the current barrel in +0xb4, advancing it modulo the count before each
shot, and launches from that node's world matrix.

Muzzle flashes: ``data/z_zbattle/particle/po2info/muzlle_pos.dat`` (loader
0x509f00) holds 95 ``[count][count node bytes]`` entries in descriptor order,
47 arms then 48 shoulders. The flash builder 0x50d600 gives each of the four
mounts two instances of ``particle/wepon/model/mazl_01.po2`` at alpha 0.4 and
0.8, scaled by the mount's base (0.9 arms, 1.1 shoulders from 0x59e740) times
1.3 or 1.7 for listed weapon ids, attached to the first flash node. Firing
(0x511630) shows both for one effect update (+0x88 = 1; 0x511810 counts it
down and hides them) and, for six arm ids, attaches them to the second flash
node when the shot left from any barrel but the first. A node callback
(0x511780) rolls the flash by a random angle in [-pi, pi] about its z axis.

Weapon ids are the robot's part ids (+0xf48 table): arm descriptor index, or
0x30 plus the shoulder descriptor index. Two listed ids (0x2f, 0x61) name no
descriptor and are kept as recovered.
"""

from __future__ import annotations

import math
import struct

import numpy as np

from .motion_resources import decode_motion_resources

MUZZLE_CALLBACK = 0x4e6db0
SHOULDER_ID_BASE = 0x30
BASE_SCALES = 0x59e740  # four floats, mounts 0..3 (arm, arm, shoulder, shoulder)
FLASH_TABLE = "MUZLLE_POS.DAT"
HIT_SCALE_TABLE = "HITEFF_SCALE.DAT"
FLASH_MODEL = "slai/assets/models/mazl_01.gltf"
BATTLE_UPDATE_HZ = 30  # libgf interval 2 in 60 Hz mode; 0x50e500 runs once per update
FLASH_ALPHAS =(0.4, 0.8)  # 0x50da04 (0x3ecccccd) and 0x50da7c (0x3f4ccccd)
LARGE_IDS = (0, 1, 0x2f, 0x30, 0x11, 0x40, 0x61)  # 0x50d888..8b8
LARGER_IDS = (9, 0x38, 0x5f, 0x2b, 0x5a)  # 0x50d8d0..8fc
BARREL_FLASH_IDS = (1, 3, 0x11, 0xe, 0x19, 0x1d)  # 0x511634..680

GUARDS = {
    # 0x509f00: table path, 95 entries, counts at +0x3684, nodes at +0x3508.
    0x509f38: 0x2484d420, 0x509f58: 0x26843508, 0x509f6c: 0x2aa2005f,
    0x509f78: 0x26643684, 0x509fa0: 0xa0623684, 0x509fc4: 0xa0433508,
    # 0x50d600: index arithmetic, multipliers, alphas and mesh flag 0x1000.
    0x50d850: 0x28a6002f, 0x50d860: 0x24a4ffd1, 0x50d874: 0x24a4ffd0,
    0x50d878: 0x2484002f, 0x50d888: 0x2ca10002, 0x50d890: 0x24a6ffd1,
    0x50d8a0: 0x24060011, 0x50d8a8: 0x24060040, 0x50d8b4: 0x24060061,
    0x50d8c0: 0x3c063fa6, 0x50d8c4: 0x34c66666,
    0x50d8d0: 0x24060009, 0x50d8d8: 0x24060038, 0x50d8e4: 0x2406005f,
    0x50d8ec: 0x2406002b, 0x50d8f8: 0x2406005a,
    0x50d908: 0x3c053fd9, 0x50d90c: 0x34a5999a,
    0x50d978: 0x80a43684, 0x50d984: 0x80c43508, 0x50d980: 0xa2400035,
    0x50da04: 0x3c023ecc, 0x50da14: 0x3442cccd, 0x50da7c: 0x3c033f4c,
    0x50da80: 0x3465cccd, 0x50da60: 0x3042fffe, 0x50dac4: 0x34c61000,
    # 0x511630: barrel-dependent flash node ids, then shown for one update.
    0x511634: 0x24020001, 0x51164c: 0x10e2000e, 0x511654: 0x24020003,
    0x51165c: 0x24020011, 0x511668: 0x2402000e, 0x511670: 0x24020019,
    0x51167c: 0x2402001d, 0x511758: 0x24030001, 0x51175c: 0xa2030038,
    # 0x511810: count down, hide at zero.
    0x5118a8: 0x2463ffff, 0x5118ac: 0xa2230038, 0x5118c4: 0x3063fffe,
    # 0x504b30 muzzle list and 0x5056e0 barrel cycling.
    0x504d40: 0x24636db0, 0x504d74: 0xa4720116, 0x504d80: 0xa6030114,
    0x50571c: 0xac8000b4, 0x505738: 0xac8200b4, 0x50574c: 0x84460116,
}


def _guard(program) -> None:
    for address, word in GUARDS.items():
        if program.read(address, 4) != struct.pack("<I", word):
            raise ValueError("Unsupported compiled S.L.A.I. muzzle code")


def _word_float(program, address: int) -> float:
    """A float built by lui/ori at `address` and `address + 4`."""
    high, low = struct.unpack("<2I", program.read(address, 8))
    return struct.unpack("<f", struct.pack("<I", (high & 0xffff) << 16 | (low & 0xffff)))[0]


def read_flash_nodes(raw: bytes, count: int) -> list[list[int]]:
    """`count` entries of [n][n node bytes]; the table holds at most four nodes."""
    out, at = [], 0
    for _ in range(count):
        if at >= len(raw) or raw[at] > 4 or at + 1 + raw[at] > len(raw):
            raise ValueError("Unsupported S.L.A.I. muzzle flash table")
        out.append(list(raw[at + 1:at + 1 + raw[at]]))
        at += 1 + raw[at]
    if at != len(raw):
        raise ValueError("Unexpected S.L.A.I. muzzle flash table length")
    return out


def read_hit_scales(raw: bytes) -> list[dict]:
    """0x509d80 / 0x50b150: per hit type, a scale interpolated over a value.

    Below `low` the scale is `low_scale`, above `high` it is `high_scale`,
    and linear between. Record n serves hit type n + 1 (0x50b150).
    """
    if len(raw) % 16:
        raise ValueError("Unsupported S.L.A.I. hit effect scale table")
    return [dict(zip(("low", "high", "low_scale", "high_scale"),
                     (lo, hi, round(a, 6), round(b, 6))))
            for lo, hi, a, b in struct.iter_unpack("<2i2f", raw)]


def flash_multiplier(source_id: int, large: float, larger: float) -> float:
    return larger if source_id in LARGER_IDS else large if source_id in LARGE_IDS else 1.0


def decode_muzzles(program, disc) -> dict:
    _guard(program)
    tables = decode_motion_resources(program)["tables"]
    rows = [("arm", r) for r in tables["arms"]] + [("shoulder", r) for r in tables["shoulders"]]
    flash = read_flash_nodes(disc.read(disc.find(FLASH_TABLE)[0]), len(rows))
    hits = read_hit_scales(disc.read(disc.find(HIT_SCALE_TABLE)[0]))
    bases = struct.unpack("<4f", program.read(BASE_SCALES, 16))
    large, larger = _word_float(program, 0x50d8c0), _word_float(program, 0x50d908)
    weapons = []
    for (mount, row), nodes in zip(rows, flash):
        index = row["index"]
        source_id = index if mount == "arm" else SHOULDER_ID_BASE + index
        base = bases[0] if mount == "arm" else bases[2]
        weapons.append({
            "mount": mount, "descriptor": index, "source_id": source_id,
            "source_model": row["model"],
            "muzzle_nodes": [i for i, c in enumerate(row["node_callbacks"])
                             if c == hex(MUZZLE_CALLBACK)],
            "flash_nodes": nodes,
            "barrel_flash": source_id in BARREL_FLASH_IDS,
            "flash_scale": round(base * flash_multiplier(source_id, large, larger), 6)})
    return {
        "flash": {"model": FLASH_MODEL, "layers": [{"alpha": a} for a in FLASH_ALPHAS],
                  "colour": [1.0, 1.0, 1.0], "visible_updates": 1, "update_hz": BATTLE_UPDATE_HZ,
                  "roll": [-math.pi, math.pi]},
        "weapons": weapons,
        "hit_effect_scales": hits,
        "adapter": ("Gameplay sockets use rest-pose muzzle nodes under native arm bone 0 or the "
                    "shoulder mount; flashes follow the animated source node"),
        "provenance": {
            "muzzle_list": "0x504b30 (callback 0x4e6db0 -> +0x116, count +0x114)",
            "barrel": "0x5056e0 (+0xb4 = (+0xb4 + 1) % count before a shot)",
            "flash_table": "0x509f00 muzlle_pos.dat -> +0x3684 counts, +0x3508 nodes",
            "flash_builder": "0x50d600 (mazl_01.po2 with mflash textures, two layers)",
            "flash_fire": "0x511630 (+0x88 = 1, visible bit, second node by barrel)",
            "flash_update": "0x511810 (count down; hide at zero)",
            "flash_roll": "0x511780 (random [-pi, pi] about z each update)",
            "hit_effect_scale": "0x509d80 loader, 0x50b150 interpolation"}}


def flash_entry(weapon: dict, mount: str, model: int) -> dict:
    """The port's flash record for a registered weapon type (WeaponTuning::model)."""
    keys = ("source_model", "source_id", "muzzle_nodes", "flash_nodes", "barrel_flash", "flash_scale")
    return {"mount": mount, "model": model, **{key: weapon[key] for key in keys}}


def _local(node: dict) -> np.ndarray:
    if "matrix" in node:
        return np.array(node["matrix"], float).reshape(4, 4).T
    x, y, z, w = node.get("rotation", [0.0, 0.0, 0.0, 1.0])
    rotation = np.array([[1 - 2 * (y * y + z * z), 2 * (x * y - z * w), 2 * (x * z + y * w)],
                         [2 * (x * y + z * w), 1 - 2 * (x * x + z * z), 2 * (y * z - x * w)],
                         [2 * (x * z - y * w), 2 * (y * z + x * w), 1 - 2 * (x * x + y * y)]])
    out = np.eye(4)
    out[:3, :3] = rotation * np.array(node.get("scale", [1.0, 1.0, 1.0]))
    out[:3, 3] = node.get("translation", [0.0, 0.0, 0.0])
    return out


def node_positions(model: dict) -> dict[int, np.ndarray]:
    """Rest-pose position of every node under the model's scene roots."""
    out = {}
    nodes = model["nodes"]

    def visit(index: int, parent: np.ndarray) -> None:
        world = parent @ _local(nodes[index])
        out[index] = world[:3, 3]
        for child in nodes[index].get("children", []):
            visit(child, world)

    for root in model["scenes"][model.get("scene", 0)]["nodes"]:
        visit(root, np.eye(4))
    return out


def muzzle_mount(model: dict, muzzle_nodes: list[int], template: dict, arm: bool) -> dict | None:
    """Weapon-mount sockets at the source muzzle nodes, in the drawn model's frame.

    The port draws an imported arm at its arm bone 0 and a shoulder at its
    shoulder mount, so each socket sits at the node's rest position under
    that frame (game space negates glTF z). Animated barrels and source body
    mounts are not followed: a labelled rest-pose adapter.
    """
    if not muzzle_nodes:
        return None
    positions = node_positions(model)
    identity = {"translation": [0.0, 0.0, 0.0], "rotation": [0.0, 0.0, 0.0],
                "scale": [1.0, 1.0, 1.0]}
    muzzles = []
    for node in muzzle_nodes:
        x, y, z = (float(v) for v in positions[node])
        muzzles.append({"position": [x, y, -z], "node": node, "bone": 0 if arm else -1,
                        "chain": [dict(identity)], "source": "slai muzzle node (rest pose)"})
    return {"resource": template.get("resource"), "muzzles": muzzles}
