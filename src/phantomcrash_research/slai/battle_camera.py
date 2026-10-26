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

In the air (actor +0xb61, the grounded flag, clear; or camera +0xf0, set by
the air movement) while rising (actor +0xe4, the vertical speed, y down, at
or below 0) or 4 updates after leaving the ground (+0xb64 counts them), the
eye stands 2 higher and the look offset shrinks to 0.75 of itself, 0.05 an
update (camera +0x4c). Otherwise it grows back to 1 at the same rate.

Legs row 33 keeps the eye at least 1.2 above the feet (0x2605a0). When the
ray meets the stage (0x2606d0..0x260a98), and the eye is within 10 of the
robot across the ground, on a surface not facing down (normal y, y down,
under 0.15), the vertical gap becomes its third plus a rise of 0.25 (0.35
for legs row 31; in the air 0.5 more, less 2.5 times the fall speed) and
the eye closes a third of that gap. A second ray from the robot to that
eye stops it again, closing a third of the gap. A further rise, 0.8 (1 - |normal y|)
times the camera's pitch (+0x44) above 0, is not carried over: the pitch's
sense against the port's is not established.
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

# 0x260344..0x26055c, the air view: the state it tests, the eye's lift, the
# look offset's scale and the call that scales it (0x10ea68).
AIR_GUARDS = {0x260344: 0x92220B61, 0x260358: 0x906300F0, 0x260368: 0xC62100E4,
              0x260380: 0x0C088C30, 0x26038C: 0xC6200B64, 0x2603C0: 0x46000801,
              0x2603C4: 0xE6000004, 0x260460: 0xE481004C, 0x260554: 0xC44C004C,
              0x26055C: 0x0C043A9A}
AIR_UPDATES_LUI = 0x260394   # 4.0 times the updates' tick count
AIR_RISE_LUI = 0x2603B0      # 2.0, off the eye's y (up is -y)
AIR_EASE_LUI = 0x260444      # 0.05 an update, with its ori
AIR_EASE_ORI = 0x26044C
AIR_LOOK_SCALE_LUI = 0x260448  # 0.75

# 0x2605a0..0x260a98, the minimum eye height and the stage response: the
# legs rows they test, the ray calls (0x4d0d50), the hit normal's y, the
# rise's subtract and multiply-subtract and the air term's speed.
WALL_GUARDS = {0x2605A0: 0x24030021, 0x2605C8: 0x46000834, 0x2605D4: 0x46001001,
               0x2605D8: 0xE6000004, 0x2606DC: 0x0C134354, 0x2607C8: 0x0C043A88,
               0x260804: 0x46010034, 0x260810: 0xC68C0034, 0x260824: 0x46006034,
               0x260864: 0x2403001F, 0x2608AC: 0x46000819, 0x2608B0: 0x4602A01D,
               0x2608F4: 0x46000819, 0x2608F8: 0x4602A01D, 0x260924: 0xC62200E4,
               0x260938: 0x46021882, 0x2609B0: 0x0C043A88, 0x2609C4: 0x0C134354}
MIN_EYE_ROW = 0x2605A0          # addiu v1, 33
MIN_EYE_LUI = (0x2605B0, 0x2605B8)    # 1.2
WALL_RISE_ROW = 0x260864        # addiu v1, 31
WALL_ROW_RISE_LUI = (0x260898, 0x26089C)  # 0.35
WALL_RISE_LUI = 0x2608E8        # 0.25
WALL_MARGIN_LUI = (0x2606FC, 0x260704)    # 0.15 off the surface
WALL_REACH_LUI = 0x2607F8        # 10.0
WALL_NORMAL_LUI = (0x260814, 0x260818)    # 0.15
WALL_EASE_LUI = (0x2607A8, 0x2607AC)      # 1/3
WALL_AIR_RISE_LUI = 0x260928    # -0.5 (up is -y)
WALL_AIR_CLIMB_LUI = 0x260920   # 2.5 times the vertical speed

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


def _lui_ori_float(program, lui, ori):
    high = struct.unpack("<I", program.read(lui, 4))[0]
    low = struct.unpack("<I", program.read(ori, 4))[0]
    if high >> 26 != 0x0F or low >> 26 != 0x0D:
        raise ValueError("Unsupported S.L.A.I. behind-view code")
    return struct.unpack("<f", struct.pack("<I", (high & 0xFFFF) << 16 | low & 0xFFFF))[0]


def _guard(program, guards):
    for address, word in guards.items():
        if program.read(address, 4) != struct.pack("<I", word):
            raise ValueError("Unsupported S.L.A.I. behind-view code")


def air_view(program):
    """The behind view's lift and closer look while the robot is in the air."""
    _guard(program, AIR_GUARDS)
    return {"rise": _lui_float(program, AIR_RISE_LUI),
            "look_scale": _lui_float(program, AIR_LOOK_SCALE_LUI),
            "ease": round(_lui_ori_float(program, AIR_EASE_LUI, AIR_EASE_ORI), 6),
            "updates": _lui_float(program, AIR_UPDATES_LUI)}


def _row_test(program, address):
    word = struct.unpack("<I", program.read(address, 4))[0]
    if word >> 16 != 0x2403:
        raise ValueError("Unsupported S.L.A.I. behind-view code")
    return word & 0xFFFF


def wall_view(program):
    """The behind view's response to the stage on its sight line."""
    _guard(program, WALL_GUARDS)
    return {"margin": round(_lui_ori_float(program, *WALL_MARGIN_LUI), 6),
            "reach": _lui_float(program, WALL_REACH_LUI),
            "normal_limit": round(_lui_ori_float(program, *WALL_NORMAL_LUI), 6),
            "ease": round(_lui_ori_float(program, *WALL_EASE_LUI), 6),
            "rise": _lui_float(program, WALL_RISE_LUI),
            "air_rise": -_lui_float(program, WALL_AIR_RISE_LUI),
            "air_climb": _lui_float(program, WALL_AIR_CLIMB_LUI)}


def chase_rows(program):
    """The behind view's eye, by legs row: how far behind and above the feet,
    with the rows' own minimum height and wall rise."""
    _guard(program, CHASE_GUARDS)
    _guard(program, WALL_GUARDS)
    min_row = _row_test(program, MIN_EYE_ROW)
    rise_row = _row_test(program, WALL_RISE_ROW)
    rise = _lui_float(program, EYE_RISE_LUI)
    back = _lui_float(program, EYE_BACK_LUI)
    raw = program.read(CHASE_TABLE, CHASE_ROW_BYTES * CHASE_ROWS)
    rows = []
    for index in range(CHASE_ROWS):
        x, y, z = struct.unpack_from("<3f", raw, index * CHASE_ROW_BYTES)
        if x != 0:
            raise ValueError("S.L.A.I. behind-view row with a sideways eye")
        row = {"row": index, "behind": round(back - z, 4), "eye_height": round(rise - y, 4)}
        if index == min_row:
            row["min_eye_height"] = round(_lui_ori_float(program, *MIN_EYE_LUI), 6)
        if index == rise_row:
            row["wall_rise"] = round(_lui_ori_float(program, *WALL_ROW_RISE_LUI), 6)
        rows.append(row)
    return rows


def leg_chase_camera(rows, record):
    """The behind view's eye for a legs record: the row 0x254890 returns."""
    index = record["alternate_model"]
    if not 0 <= index < len(rows):
        raise ValueError(f"Legs record {record['index']} has no behind-view row")
    row = rows[index]
    extra = {key: row[key] for key in ("min_eye_height", "wall_rise") if key in row}
    return {"behind": row["behind"], "eye_height": row["eye_height"], **extra,
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
            "chase_camera": {**MEASURED_BEHIND, **_look(program), "air": air_view(program),
                             "wall": wall_view(program)},
            "source": {"screen_distance": distance, "aspect_x": aspect_x, "aspect_y": aspect_y,
                       "half_frame_lines": HALF_FRAME_LINES},
            "notes": ["fov_degrees: 2 atan(224 / (screen distance 0x2bb2a0 * vertical aspect "
                      "0x2bb2a8)), set by the view-screen setup 0x1c1410 for every battle view.",
                      "chase_camera: the behind view 0x260070 looks 0.5 above and 20 ahead of "
                      "the feet. Its eye comes from the legs' row of 0x335170 (the leg type's "
                      "source_chase_camera); eye_height 5.0 and behind 8.6, measured under "
                      "PCSX2, stand in for legs without a row.",
                      "chase_camera.air: rising, or 4 updates into the air, the eye stands 2 "
                      "higher and the look offset eases to 0.75 of itself, 0.05 an update "
                      "(0x260344..0x26055c).",
                      "chase_camera.wall: a stage surface on the sight line stops the eye 0.15 "
                      "off it; within 10 of the robot the vertical gap becomes its third plus "
                      "0.25 (legs row 31 0.35, the leg type's wall_rise; 0.5 more less 2.5 "
                      "times the fall speed in the air) before the eye closes a third, and a "
                      "second ray stops it again (0x2606d0..0x260a98). Legs row 33 "
                      "keeps the eye 1.2 above the feet (min_eye_height, 0x2605a0)."]}
