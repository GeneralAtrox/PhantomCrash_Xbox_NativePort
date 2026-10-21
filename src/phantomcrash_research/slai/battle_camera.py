"""The S.L.A.I. battle camera: its projection and its behind-view offsets.

The view-screen setup at 0x1c1410 stores the screen distance from 0x2bb2a0
(549.0) at +0x1980 and the aspect factors from 0x2bb2a4 (1.0) and 0x2bb2a8
(1.16) at +0x2728/+0x272c. Its 512-line PAL mode multiplies the vertical
factor by 8/7 (0x1c149c..b8), which keeps the vertical view of the 448-line
frame, so the vertical field of view is 2 atan(224 / (549.0 * 1.16)).
PCSX2 showed the resulting view-screen matrix (x 549.0, y 727.817) and a
port frame at this angle lines up with the original's stage edges.

The behind-view offsets are runtime measurements, not recovered constants:
at rest in a New York Quick Battle the camera (0x4b2f00 + robot * 0x100)
kept its eye 5.0 above and 8.6 behind the robot's feet and its look point
0.5 above and 20 ahead of them (eye offset +0xb0, look offset +0xc0).
"""

import math
import struct

SCREEN_DISTANCE_ADDRESS = 0x2BB2A0
ASPECT_X_ADDRESS = 0x2BB2A4
ASPECT_Y_ADDRESS = 0x2BB2A8
# Instruction words of the view-screen setup: the loads of the three values,
# their stores to the draw state and the 8/7 PAL factor.
VIEW_SCREEN_GUARDS = {0x1C1438: 0xC420B2A0, 0x1C1440: 0xE6201980,
                      0x1C1448: 0xC420B2A4, 0x1C144C: 0xE6202728,
                      0x1C1454: 0xC420B2A8, 0x1C145C: 0xE620272C,
                      0x1C14A0: 0x3C024100, 0x1C14A8: 0x3C0240E0}
# Half of the 448-line frame the vertical aspect factor is set for.
HALF_FRAME_LINES = 224

# Measured under PCSX2 (adapters until the follow code is recovered).
MEASURED_BEHIND = {"eye_height": 5.0, "behind": 8.6, "height": 0.5, "look_ahead": 20.0}


def _float(program, address):
    return struct.unpack("<f", program.read(address, 4))[0]


def battle_camera(program):
    """The stages.json "battle_camera" block for S.L.A.I. arenas."""
    for address, word in VIEW_SCREEN_GUARDS.items():
        if program.read(address, 4) != struct.pack("<I", word):
            raise ValueError("Unsupported S.L.A.I. view-screen setup")
    distance = _float(program, SCREEN_DISTANCE_ADDRESS)
    aspect_x = _float(program, ASPECT_X_ADDRESS)
    aspect_y = _float(program, ASPECT_Y_ADDRESS)
    if not (distance > 0 and aspect_x > 0 and aspect_y > 0):
        raise ValueError("Invalid S.L.A.I. view-screen values")
    fov = math.degrees(2 * math.atan(HALF_FRAME_LINES / (distance * aspect_y)))
    return {"fov_degrees": round(fov, 3),
            "chase_camera": dict(MEASURED_BEHIND),
            "source": {"screen_distance": distance, "aspect_x": aspect_x, "aspect_y": aspect_y,
                       "half_frame_lines": HALF_FRAME_LINES},
            "notes": ["fov_degrees: 2 atan(224 / (screen distance 0x2bb2a0 * vertical aspect "
                      "0x2bb2a8)), set by the view-screen setup 0x1c1410 for every battle view.",
                      "chase_camera: the behind view's eye 5.0 above and 8.6 behind the feet, "
                      "looking 0.5 above and 20 ahead of them; measured at rest under PCSX2, "
                      "the follow code that sets them is not recovered."]}
