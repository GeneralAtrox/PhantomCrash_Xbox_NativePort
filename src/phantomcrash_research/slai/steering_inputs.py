"""Original S.L.A.I. steering inputs: the filtered turn control (actor +0xe18),
the loaded turn speed (+0x8c), the turn motion's playback rate and the frame
cadence the battle callbacks run at (PAL SLES_529.40)."""

import struct

from .program import ProgramData

# Pad reading and the shared right stick (player pad update 0x4de690..4df13c).
PAD_GUARDS = {
    0x221444: 0x2403007f,  # a digital source reads full scale, 0x7f
    0x1b6ea8: 0x24050037,  # stick dead zone, 55 of 128, every axis
    0x4de760: 0x26240e10,  # not pad-driven: clear +0xe10..e37 ...
    0x4de768: 0x0c04d5f8,  # ... memset
    0x4de76c: 0x24060028,  # ... 0x28 bytes
    0x4de7b4: 0x3c0242fe,  # +0xe18 = (positive + negative source) / 127.0
    0x4de7dc: 0xe6200e18,
    0x4de9b8: 0x8e220ea0,  # a mapped right-stick x (+0xea0) skips the shared filter
    0x4debdc: 0xc62d0e18,  # angle of +0xe18 against -+0xe2c
    0x4debe0: 0x0c06a884,  # 0x1aa210, atan2
    0x4debf4: 0x3c033f06, 0x4debf8: 0x34630a92,  # pi/6
    0x4dec18: 0x3c034027, 0x4dec1c: 0x34638d36,  # 5pi/6
    0x4dec3c: 0xae200e18,  # turn dropped
    0x4dec48: 0x3c033f9c, 0x4dec4c: 0x346361aa,  # 70 degrees
    0x4dec6c: 0x3c033ff5, 0x4dec70: 0x3463be0b,  # 110 degrees
    0x4dec88: 0xae200e2c,  # aim dropped
    0x4deca4: 0x3c033edc,  # minus 55/128 ...
    0x4decc0: 0x3c033f12,  # ... over 73/128
}
# 0x4dd590 (from 0x4ddd9c after 0x24dba0): the legs' stats under the load.
LOAD_GUARDS = {
    0x4ddd9c: 0x0c137564,
    0x4dd5b8: 0x0c095240,  # 0x254900 total weight
    0x4dd5c8: 0x0c095918,  # 0x256460 frame, body and legs at level 0
    0x4dd5d0: 0x86040006,  # legs level, assembly +6
    0x4dd5dc: 0x0c095a34,  # 0x2568d0 speed and turn factor
    0x4dd62c: 0x0c0959c0,  # 0x256700 clamps
    0x4dd8d8: 0xc7a00058, 0x4dd8dc: 0xe620008c,  # +0x8c
    0x256838: 0x3c023dcc, 0x25683c: 0x3442cccd,  # turn at least 0.1 ...
    0x256844: 0x3c0242b4,  # ... at most 90
    0x2568ec: 0x3c023f80,  # no weight: 1
    0x256934: 0x3c0242c6,  # level / 99 ...
    0x256944: 0x3c023dcc, 0x256948: 0x3442cccd,  # ... times 0.1
    0x256994: 0x3c02420c,  # overload band 35
    0x2569b8: 0x26100001,  # counted from 1
    0x2569bc: 0x3c024120,  # power 10 ...
    0x2569cc: 0x2a010004,  # ... unless the band is under 4 ...
    0x2569d8: 0x02101018,  # ... then the band squared
    0x256a08: 0x0c04b366,  # powf
}
# 0x4ebc28..4ebcd4: the turn state's motion rate.
PLAYBACK_GUARDS = {
    0x4ebc28: 0x0c135eac,  # vsyncs a frame
    0x4ebc34: 0xc601008c, 0x4ebc38: 0x3c023c8e, 0x4ebc3c: 0x3442fa35,
    0x4ebc48: 0xc6000e18,
    0x4ebc54: 0x0c04b27e,  # fabs
    0x4ebc5c: 0x3c023da0, 0x4ebc64: 0x3442d97c,  # pi/40
    0x4ebc68: 0x3c034000,  # halved
    0x4ebc7c: 0x3c023f66, 0x4ebc80: 0x34426666,  # at least 0.9
    0x4ebcb0: 0x3c023f8c, 0x4ebcb4: 0x3442cccd,  # at most 1.1
    0x4ebcd0: 0x0c13a594,  # 0x4e9650 sets the rate
}
# The display object's frame interval: two vsyncs a game frame.
CADENCE_GUARDS = {
    0x1a86ec: 0xac880010,  # +0x10 = interval
    0x1cbc84: 0x24080002, 0x269494: 0x24080002,
    0x4dd6b4: 0x2403003c,  # per-second values over 60 / interval
    0x4e55f4: 0xc662008c, 0x4e55fc: 0xc6600e18, 0x4e5614: 0x46010042,
    0x4e56dc: 0xc6610e18, 0x4e56fc: 0x46000d43,
}
GUARDS = {**PAD_GUARDS, **LOAD_GUARDS, **PLAYBACK_GUARDS, **CADENCE_GUARDS}


def _float(word):
    return struct.unpack('<f', struct.pack('<I', word))[0]


def _hi_lo(program, hi, lo=None):
    upper = struct.unpack('<I', program.read(hi, 4))[0] & 0xffff
    lower = struct.unpack('<I', program.read(lo, 4))[0] & 0xffff if lo else 0
    return _float(upper << 16 | lower)


def decode_steering_inputs(program):
    for address, word in GUARDS.items():
        if program.read(address, 4) != struct.pack('<I', word):
            raise ValueError(f'Unsupported S.L.A.I. steering input at {address:#x}')
    dead_zone = struct.unpack('<I', program.read(0x1b6ea8, 4))[0] & 0xffff
    return {
        'pad': {'digital_scale': 0x7f, 'stick_dead_zone': dead_zone,
                'turn_scale': _hi_lo(program, 0x4de7b4)},
        'shared_stick': {'turn_dead_angle': _hi_lo(program, 0x4debf4, 0x4debf8),
                         'turn_dead_far': _hi_lo(program, 0x4dec18, 0x4dec1c),
                         'aim_dead_low': _hi_lo(program, 0x4dec48, 0x4dec4c),
                         'aim_dead_high': _hi_lo(program, 0x4dec6c, 0x4dec70),
                         'rescale_offset': _hi_lo(program, 0x4deca4),
                         'rescale_range': _hi_lo(program, 0x4decc0),
                         'layouts': {'standard': 3, 'clear': 2}},
        'release': 'none: unmapped controls clear +0xe10..e37 and pads recompute them each frame',
        'source_load': {'level_max': round(_hi_lo(program, 0x256934)),
                        'level_trim': _hi_lo(program, 0x256944, 0x256948),
                        'overload_band': _hi_lo(program, 0x256994),
                        'graded_bands': (struct.unpack('<I', program.read(0x2569cc, 4))[0] & 0xffff) - 1,
                        'overload_power': _hi_lo(program, 0x2569bc)},
        'turn_speed': {'min': _hi_lo(program, 0x256838, 0x25683c),
                       'max': _hi_lo(program, 0x256844)},
        'playback': {'step': _hi_lo(program, 0x4ebc5c, 0x4ebc64),
                     'min': _hi_lo(program, 0x4ebc7c, 0x4ebc80),
                     'max': _hi_lo(program, 0x4ebcb0, 0x4ebcb4)},
        'cadence': {'source_tick_rate': 60,
                    'source_frame_ticks': struct.unpack('<I', program.read(0x1cbc84, 4))[0] & 0xffff},
        'provenance': {'pad': '0x221320, 0x1b7550, 0x4de778..7dc',
                       'shared_stick': '0x4de9b8, 0x4debd8..4dede0, layouts 0x2206d0/0x2208c8',
                       'turn_speed': '0x4dd590, 0x256700, 0x2568d0',
                       'playback': '0x4ebc28..4ebcd4',
                       'cadence': '0x1a86e0, 0x1cbc84, 0x269494, 0x4e55e0..569c, 0x4e56f4..56fc',
                       'adapters': 'Host stick dead zone and rescale stand in for the pad dead zone '
                                   'and 55/128..73/128 rescale; the native wheel mover keeps strafe '
                                   'steering and its heading rate'}}


def read_steering_inputs(disc):
    return decode_steering_inputs(ProgramData(disc))
