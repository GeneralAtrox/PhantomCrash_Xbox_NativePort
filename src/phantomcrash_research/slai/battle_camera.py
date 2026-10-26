"""The S.L.A.I. battle camera: its projection and its behind-view offsets.

The view-screen setup at 0x1c1410 stores the screen distance from 0x2bb2a0
(549.0) at +0x1980 and the aspect factors from 0x2bb2a4 (1.0) and 0x2bb2a8
(1.16) at +0x2728/+0x272c. Its 512-line PAL mode multiplies the vertical
factor by 8/7 (0x1c149c..b8), which keeps the vertical view of the 448-line
frame, so the vertical field of view is 2 atan(224 / (549.0 * 1.16)).
PCSX2 showed the resulting view-screen matrix (x 549.0, y 727.817) and a
port frame at this angle lines up with the original's stage edges.

The behind view is camera mode 1 (0x260070, entry 1 of the mode table
0x335360). It reads the row of the table at 0x335170 that 0x254890 picks
(the legs record's +6, as the arm aim does) and puts the eye at the row
plus (0, -4.5, -12) and the look point at (0, -0.5, 20) from the feet, in
source axes (y down, z ahead), turned by the camera's pitch and heading.
A ray from the robot to the eye stops at the stage (0x4d0d50, 0.15 off the
surface) and the eye closes a third of its gap a frame. Row x is the eye's
sideways offset; every shipped row has none.
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

CHASE_TABLE = 0x335170
CHASE_ROW_BYTES = 12
CHASE_ROWS = 35
# 0x260070: the row's address (0x335170 + 12 row, x/y/z), then the float
# immediates (lui) the eye and look point are built from.
CHASE_GUARDS = {0x2600B4: 0x3C040033, 0x2600BC: 0x24845170, 0x2600C8: 0x24635174,
                0x2600E8: 0x24425178, 0x260100: 0x46021B41, 0x260114: 0x46000B81,
                0x2600A0: 0x0C088E78, 0x2600A8: 0x0C095224}
EYE_RISE_LUI = 0x2600CC     # 4.5, taken from the row's y (up is -y)
EYE_BACK_LUI = 0x2600F4     # 12.0, taken from the row's z (ahead is +z)
LOOK_RISE_LUI = 0x260118    # -0.5
LOOK_AHEAD_LUI = 0x260128   # 20.0
CHASE_PROVENANCE = {"code": "0x260070 (camera mode 1 of 0x335360)", "table": "0x335170",
                    "row_select": "0x2600a8 (0x254890: legs record +6)",
                    "eye": "row + (0, -4.5, -12)", "look": "(0, -0.5, 20)"}

# Measured under PCSX2: a robot whose legs have no row (another maker's).
MEASURED_BEHIND = {"eye_height": 5.0, "behind": 8.6}


def _float(program, address):
    return struct.unpack("<f", program.read(address, 4))[0]


def _lui_float(program, address):
    word = struct.unpack("<I", program.read(address, 4))[0]
    if word >> 26 != 0x0F:
        raise ValueError("Unsupported S.L.A.I. behind-view code")
    return struct.unpack("<f", struct.pack("<I", (word & 0xFFFF) << 16))[0]


def _look(program):
    return {"height": -_lui_float(program, LOOK_RISE_LUI),
            "look_ahead": _lui_float(program, LOOK_AHEAD_LUI)}


def chase_rows(program):
    """The behind view's eye, by legs row: how far behind and above the feet."""
    for address, word in CHASE_GUARDS.items():
        if program.read(address, 4) != struct.pack("<I", word):
            raise ValueError("Unsupported S.L.A.I. behind-view code")
    rise = _lui_float(program, EYE_RISE_LUI)
    back = _lui_float(program, EYE_BACK_LUI)
    raw = program.read(CHASE_TABLE, CHASE_ROW_BYTES * CHASE_ROWS)
    rows = []
    for index in range(CHASE_ROWS):
        x, y, z = struct.unpack_from("<3f", raw, index * CHASE_ROW_BYTES)
        if x != 0:
            raise ValueError("S.L.A.I. behind-view row with a sideways eye")
        rows.append({"row": index, "behind": round(back - z, 4), "eye_height": round(rise - y, 4)})
    return rows


def leg_chase_camera(rows, record):
    """The behind view's eye for a legs record: the row 0x254890 returns."""
    index = record["alternate_model"]
    if not 0 <= index < len(rows):
        raise ValueError(f"Legs record {record['index']} has no behind-view row")
    row = rows[index]
    return {"behind": row["behind"], "eye_height": row["eye_height"],
            "provenance": {**CHASE_PROVENANCE, "row": index}}


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
    chase_rows(program)
    fov = math.degrees(2 * math.atan(HALF_FRAME_LINES / (distance * aspect_y)))
    return {"fov_degrees": round(fov, 3),
            "chase_camera": {**MEASURED_BEHIND, **_look(program)},
            "source": {"screen_distance": distance, "aspect_x": aspect_x, "aspect_y": aspect_y,
                       "half_frame_lines": HALF_FRAME_LINES},
            "notes": ["fov_degrees: 2 atan(224 / (screen distance 0x2bb2a0 * vertical aspect "
                      "0x2bb2a8)), set by the view-screen setup 0x1c1410 for every battle view.",
                      "chase_camera: the behind view 0x260070 looks 0.5 above and 20 ahead of "
                      "the feet. Its eye comes from the legs' row of 0x335170 (the leg type's "
                      "source_chase_camera); eye_height 5.0 and behind 8.6, measured under "
                      "PCSX2, stand in for legs without a row."]}
