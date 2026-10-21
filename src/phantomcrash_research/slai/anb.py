"""S.L.A.I.'s ``ANB`` 2D animation banks, read and written.

Layout, recovered from the sprite engine (researcher labels; see
docs/SLAI_CAMPAIGN_RESEARCH.md, "Talk scene assets"):

- header: u32 0x01235678, ``ANB\\0``, u32 version 0x78, u32 texture count,
  u32 cell count, u32 frame count, u32 anim count, then the offsets of the
  cells, the frame table and the anim table. The loader 0x1CD610 turns the
  offsets into pointers and renames ``ANB`` to ``ANT``.
- cell (0x54 bytes): i32 texture, i32 unused, f32 u0 v0 u1 v1 (texels),
  f32 x y (pivot position, y up), f32 half width/height, f32 scale x/y,
  f32 rotation (degrees), f32 origin x/y, f32 second scale x/y, f32 second
  rotation, i32 blend (0 opaque, 1 alpha, 2 additive), i32 unused, u8 RGBA
  (0..255). Drawn by 0x1CDD30.
- frame: i32 cell indices ending in -1, drawn last to first (0x1CDBE0).
- anim: 16-byte commands {i32 type, f32 a, f32 b, f32 c} stepped by 0x1CD8C0:
  0 show frame a for b ticks; 1 tween to the next frame over a*b ticks;
  2 loop (or end); 3 jump to anim a; -1 end.
"""

from __future__ import annotations

import struct

MAGIC = 0x01235678
TAG = b"ANB\0"
VERSION = 0x78
HEADER = struct.Struct("<I4s8I")
CELL = struct.Struct("<2i16f2i4B")
COMMAND = struct.Struct("<i3f")
COMMANDS = {0: "frame", 1: "tween", 2: "loop", 3: "jump", -1: "end"}
_TYPES = {name: value for value, name in COMMANDS.items()}
BLENDS = ("opaque", "alpha", "additive")
_FLOATS = ("u0", "v0", "u1", "v1", "x", "y", "half_width", "half_height", "scale_x",
           "scale_y", "rotation", "origin_x", "origin_y", "scale2_x", "scale2_y",
           "rotation2")


def _number(value: float) -> float | int:
    return int(value) if value == int(value) and abs(value) < 1 << 24 else value


def _list_at(data: bytes, at: int) -> list[int]:
    out = []
    while True:
        value = struct.unpack_from("<i", data, at)[0]
        if value == -1:
            return out
        out.append(value)
        at += 4


def read_anb(data: bytes) -> dict:
    (magic, tag, version, textures, cell_count, frame_count, anim_count, at_cells,
     at_frames, at_anims) = HEADER.unpack_from(data)
    if magic != MAGIC or tag not in (TAG, b"ANT\0"):
        raise ValueError("Not an ANB animation bank")
    cells = []
    for index in range(cell_count):
        values = CELL.unpack_from(data, at_cells + index * CELL.size)
        cell = {"texture": values[0]}
        cell.update({name: _number(v) for name, v in zip(_FLOATS, values[2:18])})
        if not 0 <= values[18] < len(BLENDS):
            raise ValueError(f"ANB cell {index}: unknown blend {values[18]}")
        cell["blend"] = BLENDS[values[18]]
        cell["rgba"] = list(values[20:24])
        cells.append(cell)
    frames = []
    for index in range(frame_count):
        at = struct.unpack_from("<I", data, at_frames + 4 * index)[0]
        frames.append(_list_at(data, at) if at else None)
    anims = []
    for index in range(anim_count):
        at = struct.unpack_from("<I", data, at_anims + 4 * index)[0]
        commands = []
        while True:
            kind, a, b, c = COMMAND.unpack_from(data, at)
            if kind not in COMMANDS:
                raise ValueError(f"ANB anim {index}: unknown command {kind}")
            command = {"op": COMMANDS[kind]}
            if kind in (0, 1):
                command.update(a=_number(a), b=_number(b))
            elif kind == 3:
                command["anim"] = int(a)
            if c:
                command["c"] = _number(c)
            commands.append(command)
            at += COMMAND.size
            if kind == -1:
                break
        anims.append(commands)
    return {"version": version, "textures": textures, "cells": cells, "frames": frames,
            "anims": anims}


def write_anb(bank: dict) -> bytes:
    """Inverse of read_anb: the same layout, with relative offsets."""
    cells, frames, anims = bank["cells"], bank["frames"], bank["anims"]
    at_cells = HEADER.size
    at_frames = at_cells + CELL.size * len(cells)
    at_anims = at_frames + 4 * len(frames)
    body = bytearray()
    cursor = at_anims + 4 * len(anims)
    frame_offsets = []
    for frame in frames:
        if frame is None:
            frame_offsets.append(0)
            continue
        frame_offsets.append(cursor + len(body))
        body += struct.pack(f"<{len(frame) + 1}i", *frame, -1)
    anim_offsets = []
    for commands in anims:
        anim_offsets.append(cursor + len(body))
        for command in commands:
            kind = _TYPES[command["op"]]
            a = command.get("anim", command.get("a", 0))
            body += COMMAND.pack(kind, a, command.get("b", 0), command.get("c", 0))
    out = bytearray(HEADER.pack(MAGIC, TAG, bank.get("version", VERSION), bank["textures"],
                                len(cells), len(frames), len(anims), at_cells, at_frames,
                                at_anims))
    for cell in cells:
        floats = [float(cell[name]) for name in _FLOATS]
        out += CELL.pack(cell["texture"], 0, *floats, BLENDS.index(cell["blend"]), 0,
                         *cell["rgba"])
    out += struct.pack(f"<{len(frames)}I", *frame_offsets)
    out += struct.pack(f"<{len(anims)}I", *anim_offsets)
    return bytes(out + body)
