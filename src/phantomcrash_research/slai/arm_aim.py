"""Original arm aim: the upper-arm raise and the weapon's look-at.

0x504b30 binds 0x4e7a90 to body node 4 for weapon slot 0 and node 2 for
slot 1 (their children 5 and 3 are the hands, descriptor attachments 1 and
0), and 0x4e7f60 to the weapon node whose descriptor callback is 0x4e6dc0;
0x4e6db0 marks the muzzle nodes that 0x5056e0 fires from.

0x4e7a90 reads the row of the table at 0x59a910 that 0x254890 picks: the
legs record's +6 (the legs descriptor row). A row is, per slot, an x angle
offset by the target's pitch and clamped, fixed y and z angles, then the
axis order, a flag that keeps the node's own rotation, and the weapon twist
0x4e7f60 adds about the barrel (negated for slot 0). Both callbacks weight
by the raise counter 0x4dfde8 steps once a tick towards +0x136's frames.
Field labels are ours; the names of the original fields are not shipped.
"""

import struct

ARM_AIM_TABLE = 0x59A910
ROW_BYTES = 0x30
ROWS = 35
ARM_AIM_NODES = (4, 2)  # by weapon slot; port side 0 is source slot 0
WEAPON_AIM_CALLBACK = 0x4E6DC0
MUZZLE_CALLBACK = 0x4E6DB0
AXES = {1: "x", 2: "y", 3: "z"}
GUARDS = {0x4E7AF0: 0x8E440F48, 0x4E7B00: 0x2484A910, 0x4E7B3C: 0x26450120,
          0x4E7B44: 0x26460CD0, 0x4E80E4: 0xC66C002C, 0x4DFDE8: 0x8E040EAC,
          0x4DFDEC: 0x844300A4,
          # The cleared flag's path: +0x150, +0x15c, RotY, inverse, product.
          0x4E7DA8: 0x9222002B, 0x4E7DB4: 0xC6410150, 0x4E7DB8: 0xC640015C,
          0x4E7E78: 0x0C043B46, 0x4E7ED8: 0x0C043A52, 0x4E7EE8: 0x0C043A0C}
PROVENANCE = {"table": "0x59a910", "row_select": "0x4e7aec (0x254890: legs record +6)",
              "arm_aim": "0x4e7a90", "weapon_aim": "0x4e7f60", "raise_counter": "0x4dfde8",
              "binding": "0x504b30", "fields": "researcher assigned"}


def arm_aim_rows(program):
    for address, word in GUARDS.items():
        if program.read(address, 4) != struct.pack("<I", word):
            raise ValueError("Unsupported S.L.A.I. arm aim code")
    raw = program.read(ARM_AIM_TABLE, ROW_BYTES * ROWS)
    rows = []
    for index in range(ROWS):
        row = raw[index * ROW_BYTES:(index + 1) * ROW_BYTES]
        values = struct.unpack_from("<10f", row)
        twist = struct.unpack_from("<f", row, 0x2C)[0]
        order = [AXES[a] for a in row[0x28:0x2B] if a in AXES]
        sides = []
        for slot, base in ((0, 5), (1, 0)):
            pitch, yaw, roll, low, high = (round(v, 6) for v in values[base:base + 5])
            sides.append({"node": ARM_AIM_NODES[slot], "pitch": pitch, "yaw": yaw, "roll": roll,
                          "pitch_limits": [low, high],
                          "twist": round(twist if slot == 1 else -twist, 6)})
        # A zero flag (0x4e7da8) sets the node's world rotation outright: the
        # turn, then RotY of the actor's +0x150 heading plus its +0x15c
        # (0x4e7db4..4e7f14). The port takes the body frame's yaw for both.
        rows.append({"row": index, "sides": sides, "order": order,
                     "keep_rotation": row[0x2B] != 0})
    return rows


def weapon_nodes(profile):
    callbacks = [int(c, 0) if c else 0 for c in profile["node_callbacks"]]
    aim = next((n for n, c in enumerate(callbacks) if c == WEAPON_AIM_CALLBACK), -1)
    muzzles = [n for n, c in enumerate(callbacks) if c == MUZZLE_CALLBACK]
    return aim, muzzles


def leg_arm_aim(rows, record):
    """The row 0x254890 returns for a legs record."""
    index = record["alternate_model"]
    if not 0 <= index < len(rows):
        raise ValueError(f"Legs record {record['index']} has no arm aim row")
    return {**rows[index], "provenance": PROVENANCE}


def body_arm_aim(rows, bodies, stem):
    """Adapter for another maker's legs: the body's first own legs row."""
    index = next(row["index"] for row in bodies if row["model"].upper() == stem.upper() + ".PO2")
    return {**rows[index], "provenance": {**PROVENANCE, "row_select":
                                          "adapter: first legs descriptor row of this body"}}
