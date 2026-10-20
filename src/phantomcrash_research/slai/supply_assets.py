"""Authored supply-ship paths and the crate's compiled presentation values."""

import math
import struct
from pathlib import PurePosixPath

from .resources import resource_packages


def read_supply_path(raw, scale):
    if len(raw) < 20:
        raise ValueError('Truncated supply path')
    count, closing, drop, drop_end, leave = struct.unpack_from('<5h', raw)
    if count < 0 or len(raw) != 20 + count * 64:
        raise ValueError('Unsupported supply path size')
    if count and not (0 <= closing <= drop <= drop_end <= leave < count):
        raise ValueError('Invalid supply path boundaries')
    segments = []
    for i in range(count):
        points = []
        for j in range(4):
            x, y, z, w = struct.unpack_from('<4f', raw, 20 + i * 64 + j * 16)
            if not all(math.isfinite(v) for v in (x, y, z, w)) or w != 1:
                raise ValueError('Invalid supply path point')
            points.append([x * scale, -y * scale, -z * scale])
        segments.append(points)
    # +6 is copied to gp-0x4ddc (0x5240e0) and never read again; +12 is the
    # crates' floor (0x5240f4 stores it, 0x525058 reads it), y pointing down.
    floor = struct.unpack_from('<f', raw, 12)[0]
    return {'segments': segments, 'closing': closing, 'drop': drop,
            'drop_end_source': drop_end, 'leave': leave,
            'source_header_float': floor, 'floor': -floor * scale or 0.0}


def _float(word):
    return struct.unpack('<f', struct.pack('<I', word))[0]


def _hi(address):
    """The float a guarded lui loads."""
    return _float((FLIGHT_GUARDS[address] & 0xffff) << 16)


def _pair(hi, lo):
    """The float a guarded lui/ori pair builds."""
    return _float((FLIGHT_GUARDS[hi] & 0xffff) << 16 | FLIGHT_GUARDS[lo] & 0xffff)


def _imm(address):
    """A guarded addiu's immediate."""
    return FLIGHT_GUARDS[address] & 0xffff


def _native_angles(x, y, z):
    """Source joint angles to native: the axes flip as (1, -1, -1)."""
    return [x, -y if y else 0.0, -z if z else 0.0]


FLIGHT_GUARDS = {
    0x1a8730: 0x2406003c, 0x1a8750: 0x24030032,  # display: 60 or 50 fields
    0x52274c: 0x3c023c75, 0x522754: 0x3442c28f,  # spawn speed .015
    0x522d5c: 0x24060064, 0x522d60: 0x24050384,  # crate delay 100, life 900
    0x522d64: 0x2404000f,                        # crate burst hold 15
    0x5239b0: 0x3c024040, 0x523a24: 0x3c023f80,  # shot radii 3 and 1
    0x523b50: 0x3c023e99, 0x523b58: 0x3442999a,  # shot down: effect scale .3
    0x523bb0: 0x3c033ecc, 0x523bb8: 0x3463cccd,  # wreck colour .4
    0x523c34: 0x3c023f19, 0x523c3c: 0x3442999a,  # speed kept .6
    0x523c5c: 0x3c033e99, 0x523c64: 0x3462999a,  # first drop .3
    0x523c7c: 0x240201f4,                        # wreck timer 500
    0x523fbc: 0x8c420014, 0x523fc4: 0xac228c1c,  # respawn = libgf rate
    0x5235c8: 0x3c033cf5, 0x5235cc: 0x3463c28f,  # light step every update
    0x5240dc: 0x86020006, 0x5240e0: 0xaf82b224,  # header +6, unread
    0x5240ec: 0xc600000c, 0x5240f4: 0xe4208c10,  # header +12 -> floor
    0x5244a4: 0x3c033cf5, 0x5244ac: 0x3464c28f,  # flight light step
    0x524694: 0x3c033c8e, 0x52469c: 0x3463fa35,  # degrees to radians
    0x5246a4: 0x3c044264, 0x5246ac: 0x3c06c214,  # wing swing 57, -37
    0x5246b0: 0x3c05c130, 0x5246b4: 0x3c034130,  # -11, 11
    0x5246e0: 0x3c044214,                        # 37
    0x5248a0: 0x3c033cf5, 0x5248a4: 0x3463c28f,  # hatch step .03
    0x5248a8: 0x3c023f49, 0x5248b0: 0x34420fdb,  # hatch limit pi/4
    0x524b54: 0x3c023f81, 0x524b5c: 0x344247ae,  # wreck speed x1.01
    0x524b84: 0x3c023c23, 0x524b88: 0x3442d70a,  # joint ease .01
    0x524b70: 0x3c06005b, 0x524b7c: 0x24c63c20,  # pose table 0x5b3c20
    0x525038: 0x3c033f00, 0x525040: 0x3c02bf80,  # crate fall .5, floor - 1
    0x525058: 0xc6620320,                        # floor read
}


def supply_flight(program, scale):
    """The carrier states' values (docs/SLAI_INTEGRATION.md, 2026-10-20)."""
    for address, word in FLIGHT_GUARDS.items():
        if program.read(address, 4) != struct.pack('<I', word):
            raise ValueError(f'Unsupported supply carrier flight at {address:#x}')
    rate = _imm(0x1a8730) / 2  # libgf interval 2 (0x1cbc80, 0x269490)
    step = _pair(0x5248a0, 0x5248a4)
    degree = _pair(0x524694, 0x52469c)
    pose = struct.unpack('<28f', program.read(0x5b3c20, 112))
    # 0x5246a4..0x524788: joint 0 swings (57, -37, -11) degrees, joint 1
    # (57, 37, 11).
    swing = [(_hi(0x5246a4), _hi(0x5246ac), _hi(0x5246b0)),
             (_hi(0x5246a4), _hi(0x5246e0), _hi(0x5246b4))]
    return {
        'update_rate': rate,
        'speed': _pair(0x52274c, 0x522754),
        'release_delay': _imm(0x522d5c), 'lifetime': _imm(0x522d60),
        'burst_hold': _imm(0x522d64),
        'fall': _hi(0x525038) * scale, 'rest': -_hi(0x525040) * scale,
        'hatch_open': _pair(0x5248a8, 0x5248b0), 'hatch_speed': step,
        'light_step': _pair(0x5235c8, 0x5235cc),
        'flight_light_step': _pair(0x5244a4, 0x5244ac),
        # Joint order of 0x5f9290: the wings, tail fins, lower fins, body.
        'joint_controls': [12, 16, 9, 13, 10, 14, 0],
        'wing_swing': [_native_angles(*(v * degree for v in row)) for row in swing],
        'wreck_pose': [_native_angles(*pose[i * 4:i * 4 + 3]) for i in range(7)],
        'wreck_ease': _pair(0x524b84, 0x524b88), 'wreck_keep': _pair(0x523c34, 0x523c3c),
        'wreck_sink': _pair(0x523c5c, 0x523c64) * scale,
        'wreck_accel': _pair(0x524b54, 0x524b5c),
        'wreck_updates': _imm(0x523c7c), 'wreck_shade': _pair(0x523bb0, 0x523bb8),
        'carrier_radius': _hi(0x5239b0) * scale, 'pickup_radius': _hi(0x523a24) * scale,
        'respawn': rate,  # 0x523fbc: libgf +0x14, its updates a second
    }


def supply_assets(disc, program, units):
    guards = {0x52252c: 0x24e73b60, 0x522618: 0x26940014,
              0x522620: 0x00022180, 0x522648: 0x24060040,
              0x524438: 0x8f82b21c, 0x5245c8: 0x8f82b220,
              0x524600: 0x8f82b228, 0x525318: 0x3c024270,
              0x525334: 0x3c024170, 0x525338: 0x3c024461,
              0x5253b4: 0x3c0342c0, 0x52542c: 0x3c034280,
              0x525430: 0x3c023f00, 0x525448: 0x3c023d4c,
              0x52544c: 0x3443cccd, 0x5235c8: 0x3c033cf5,
              0x5235cc: 0x3463c28f, 0x523674: 0x0c06cf18}
    for address, word in guards.items():
        if program.read(address, 4) != struct.pack('<I', word):
            raise ValueError(f'Unsupported supply presentation at {address:#x}')
    package = resource_packages(disc)['Z/BGOBJ/SUPPLY_SHIP/JET_PATH']
    files = {f.name: f for f in disc.packages()[package]}
    counts = struct.unpack('<18i', program.read(0x5b38d0, 72))
    prefixes = [program.string(v) for v in struct.unpack('<18I', program.read(0x5b3b60, 72))]
    routes = {}
    for prefix, count in zip(prefixes, counts):
        if not count or prefix in routes:
            continue
        rows = []
        for i in range(count):
            name = PurePosixPath(prefix).name.upper() + f'{i:02d}.PTH'
            path = read_supply_path(disc.read(files[name]), units['world_scale'])
            path.update({'source': name, 'package_offset': package})
            if path['segments']:
                rows.append(path)
        routes[prefix] = rows
    return {'routes': routes,
            'carrier_model': 'slai/assets/models/jet01.gltf',
            'carrier_light_model': 'slai/assets/models/jet_light.gltf',
            'flight': supply_flight(program, units['world_scale']),
            # The pulse runs once a carrier update (0x524fc0).
            'pulse': {'frame_rate': _imm(0x1a8730) / 2, 'step': .05, 'fast_frames': 60,
                      'fast_rate': 15, 'life_numerator': 900,
                      'colour_base': .5, 'colour_steps': 96, 'colour_divisor': 64},
            'provenance': {'path_loader': '0x522520..522690',
                           'path_tables': '0x5b38d0;0x5b3b60',
                           'curve': '0x524350;0x1ab690',
                           'entry_drop_exit': '0x524438;0x5245c8;0x524600',
                           'pulse': '0x525318..525498',
                           'light': '0x5235c0..523678',
                           'states': '0x523430 update; 0x524300 flight; 0x524500 wings; '
                                     '0x5247a0 cargo; 0x523ac0 shot; 0x524ad0 wreck; '
                                     '0x524d50 parked; 0x5254d0 respawn',
                           'crates': '0x522d30 reset; 0x524fc0 update; 0x525038 fall',
                           'floor': '0x5240ec..5240f4;0x525058',
                           'unread_header_6': '0x5240dc..5240e0',
                           'cadence': 'ELF 0x1a86e0..1a8788'}}


def stage_supply_routes(assets, source):
    city = PurePosixPath(source).stem.upper().removeprefix('DAY_').removeprefix('NIGHT_')
    city = {'TOKYO': 'SINJYUKU', 'TOKYO2': 'SINJYUKU', 'ENGLAND': 'LONDON'}.get(city, city)
    return next((rows for name, rows in assets['routes'].items()
                 if PurePosixPath(name).name.upper() == city), [])
