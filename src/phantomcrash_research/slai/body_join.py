"""The original body-to-legs join: one rule for every pairing.

The PAL executable links the body model to the leg descriptor's first
attachment node and gives it unit scale with no offset. No code path reads
the pairing to adjust mount, scale or offset, so the port keeps that join and
offers only an opt-in mod adapter (data/world/body_fit.json).
"""

import struct

JAL = 3
SET_TRANSLATION = 0x1ca900  # model +0x40..48, w = 1.0
SET_ROTATION = 0x1ca8e0     # model +0x50..58
SET_SCALE = 0x1ca8d0        # model +0x60..68
ATTACH = 0x1b3590
UNIT = 0x3c023f80           # lui v0, 0x3f80 (1.0)
FULL_ASSEMBLY = (0x268810, 0x268ebc)
BATTLE_CONSTRUCTION = (0x4dc7bc, 0x4dca08)


def _word(program, address):
    data = program.read(address, 4)
    return struct.unpack('<I', data)[0] if len(data) == 4 else None


def _calls(program, start, end):
    for address in range(start, end, 4):
        word = _word(program, address)
        if word is not None and word >> 26 == JAL:
            yield address, (word & 0x3ffffff) << 2


def body_join_contract(program):
    guards = {
        # Assembly: body model (record +0xf4 -> +0xb0) to legs (+0xf8 -> +0xb0)
        # at leg descriptor +0x154, then the body's scale is set to 1.0.
        0x2689a0: 0x8ec300f4, 0x2689a4: 0x8ec200f8,
        0x2689b0: 0x0c06cd64, 0x2689b4: 0x86060154,
        0x2689bc: UNIT, 0x2689d0: 0x0c072a34, 0x2689d4: 0x46006386,
        # The link stores the parent, its node frame and the sibling chain only.
        0x1b3590: 0x8ca300c4, 0x1b359c: 0xac8500c8,
        0x1b35b8: 0xac8300cc, 0x1b35c0: 0xac8000cc,
        0x1ca8d0: 0xe48c0060, 0x1ca900: 0xe48c0040,
        # Part-model creation: every part kind gets scale 1.0, translation 0.
        0x266694: 0x8e4201a8, 0x2666a8: 0x2463dd00, 0x2666b4: 0x00400008,
        0x2666c4: UNIT, 0x2666d4: 0x0c072a34,
        0x2666e0: 0x44806000, 0x2666ec: 0x0c072a40,
        # Battle construction repeats the plain link for body and arms.
        0x4dc904: 0x0c06cd64, 0x4dc908: 0x84460154,
        0x4dc94c: 0x0c06cd64, 0x4dc950: 0x84460154,
        # Only the legs' resource root is scaled (0.1) by the part loader.
        0x2526e0: 0x3483cccd, 0x252704: 0x0c072a34, 0x252740: 0x0c072a40,
    }
    for address, word in guards.items():
        if _word(program, address) != word:
            raise ValueError(f'Unsupported source body join at {address:#x}')
    for start, end in (FULL_ASSEMBLY, BATTLE_CONSTRUCTION):
        for address, target in _calls(program, start, end):
            if target in (SET_TRANSLATION, SET_ROTATION):
                raise ValueError(f'Source body join places a part at {address:#x}')
            if target == SET_SCALE and UNIT not in [
                    _word(program, address - 4 * k) for k in range(1, 6)]:
                raise ValueError(f'Source body join scales a part at {address:#x}')
    return {'attachment': 'leg descriptor attachment entry 0 (+0x154)',
            'body_scale': 1.0, 'body_offset': [0.0, 0.0, 0.0],
            'per_pairing_rule': None,
            'provenance': {'assembly_link': '0x2689a0..9b4', 'assembly_scale': '0x2689b8..9d4',
                           'full_assembly_path': '0x268810..268ebc',
                           'attach_to_parent_node': '0x1b3590..1b3618',
                           'part_model_creation': '0x266694..2667b4;0x42dd00',
                           'battle_construction': '0x4dc8d8..4dc950',
                           'leg_root_scale': '0x2526c0..252744'}}
