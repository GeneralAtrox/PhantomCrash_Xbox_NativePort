"""Reader for S.L.A.I.'s ``P2IG`` images (``.PIG``).

Layout, found by inspection (names are researcher-assigned):

- ``P2IG``, u32 0x61, u32, u16 flags (0x0e; bit 8 clear in 707 images,
  whose pixels are stored linearly), u16 1, an 8-byte name (``S3A02H01``),
  8 bytes; at +0x20 u16 log2 width, u16 log2 height, u16 GS pixel format
  (0x13 PSMT8, 0x14 PSMT4); at +0x40 u32 palette offset, palette size,
  pixel offset, pixel size.
- The palette is RGBA8888 with alpha 0..0x80, in the GS's CSM1 order
  (entries 8-15 and 16-23 of every 32 swapped). PSMT8 pixels are stored in
  the GS's swizzled order (uploaded as 32-bit words).
- PSMT4 pixels are a PSMCT32 upload of (height / 2) x (width / 4) words that
  the GS then reads as PSMT4; this held for every 4-bit size on the disc
  from one 32x16 PSMT4 block up. Smaller ones, and those with flag bit 8
  clear (SMOKE_03, TUBU_01), are stored linearly.
  Decoding replays that through the GS page, block and column tables.
"""

from __future__ import annotations

import struct

from PIL import Image

MAGIC = b"P2IG"
PSMT8 = 0x13
PSMT4 = 0x14
SWIZZLED = 0x8


# GS local-memory layout tables (PSMCT32 and PSMT4 page arrangements).
BLOCK32 = (
    (0, 1, 4, 5, 16, 17, 20, 21),
    (2, 3, 6, 7, 18, 19, 22, 23),
    (8, 9, 12, 13, 24, 25, 28, 29),
    (10, 11, 14, 15, 26, 27, 30, 31),
)
COLUMN32 = (
    (0, 1, 4, 5, 8, 9, 12, 13),
    (2, 3, 6, 7, 10, 11, 14, 15),
    (16, 17, 20, 21, 24, 25, 28, 29),
    (18, 19, 22, 23, 26, 27, 30, 31),
    (32, 33, 36, 37, 40, 41, 44, 45),
    (34, 35, 38, 39, 42, 43, 46, 47),
    (48, 49, 52, 53, 56, 57, 60, 61),
    (50, 51, 54, 55, 58, 59, 62, 63),
)
BLOCK4 = (
    (0, 2, 8, 10),
    (1, 3, 9, 11),
    (4, 6, 12, 14),
    (5, 7, 13, 15),
    (16, 18, 24, 26),
    (17, 19, 25, 27),
    (20, 22, 28, 30),
    (21, 23, 29, 31),
)
_COLUMN4_HALF = (
    (0, 8, 32, 40, 64, 72, 96, 104, 2, 10, 34, 42, 66, 74, 98, 106,
     4, 12, 36, 44, 68, 76, 100, 108, 6, 14, 38, 46, 70, 78, 102, 110),
    (16, 24, 48, 56, 80, 88, 112, 120, 18, 26, 50, 58, 82, 90, 114, 122,
     20, 28, 52, 60, 84, 92, 116, 124, 22, 30, 54, 62, 86, 94, 118, 126),
    (65, 73, 97, 105, 1, 9, 33, 41, 67, 75, 99, 107, 3, 11, 35, 43,
     69, 77, 101, 109, 5, 13, 37, 45, 71, 79, 103, 111, 7, 15, 39, 47),
    (81, 89, 113, 121, 17, 25, 49, 57, 83, 91, 115, 123, 19, 27, 51, 59,
     85, 93, 117, 125, 21, 29, 53, 61, 87, 95, 119, 127, 23, 31, 55, 63),
    (192, 200, 224, 232, 128, 136, 160, 168, 194, 202, 226, 234, 130, 138, 162, 170,
     196, 204, 228, 236, 132, 140, 164, 172, 198, 206, 230, 238, 134, 142, 166, 174),
    (208, 216, 240, 248, 144, 152, 176, 184, 210, 218, 242, 250, 146, 154, 178, 186,
     212, 220, 244, 252, 148, 156, 180, 188, 214, 222, 246, 254, 150, 158, 182, 190),
    (129, 137, 161, 169, 193, 201, 225, 233, 131, 139, 163, 171, 195, 203, 227, 235,
     133, 141, 165, 173, 197, 205, 229, 237, 135, 143, 167, 175, 199, 207, 231, 239),
    (145, 153, 177, 185, 209, 217, 241, 249, 147, 155, 179, 187, 211, 219, 243, 251,
     149, 157, 181, 189, 213, 221, 245, 253, 151, 159, 183, 191, 215, 223, 247, 255),
)
# Rows 8-15 of a PSMT4 block repeat rows 0-7 in the block's second half.
COLUMN4 = _COLUMN4_HALF + tuple(tuple(v + 256 for v in row) for row in _COLUMN4_HALF)


def _address32(x: int, y: int, buffer_width: int) -> int:
    """GS word address of a PSMCT32 pixel; buffer width in 64-pixel units."""
    page = (y // 32) * buffer_width + x // 64
    return page * 2048 + BLOCK32[(y % 32) // 8][(x % 64) // 8] * 64 + COLUMN32[y % 8][x % 8]


def _address4(x: int, y: int, buffer_width: int) -> int:
    """GS nibble address of a PSMT4 pixel; buffer width in 64-pixel units."""
    page = (y // 128) * (buffer_width // 2) + x // 128
    return page * 16384 + BLOCK4[(y % 128) // 16][(x % 128) // 32] * 512 + COLUMN4[y % 16][x % 32]


def _upload_shape4(width: int, height: int) -> tuple[int, int]:
    return max(1, height // 2), max(1, width // 4)


def _unswizzle_palette(entries: list[bytes]) -> list[bytes]:
    if len(entries) != 256:
        return entries
    out = list(entries)
    for block in range(0, 256, 32):
        out[block + 8 : block + 16] = entries[block + 16 : block + 24]
        out[block + 16 : block + 24] = entries[block + 8 : block + 16]
    return out


def _unswizzle8(data: bytes, width: int, height: int) -> bytes:
    """PSMT8 pixels uploaded as 32-bit words back to linear order."""
    out = bytearray(width * height)
    for y in range(height):
        block_row = (y & ~0xF) * width
        swap = (((y + 2) >> 2) & 1) * 4
        column_y = (((y & ~3) >> 1) + (y & 1)) & 7
        for x in range(width):
            at = (
                block_row
                + (x & ~0xF) * 2
                + column_y * width * 2
                + ((x + swap) & 7) * 4
                + ((y >> 1) & 1)
                + ((x >> 2) & 2)
            )
            out[y * width + x] = data[at]
    return bytes(out)


def _unswizzle4(data: bytes, width: int, height: int) -> bytes:
    """PSMT4 pixels uploaded as 32-bit words back to linear indices."""
    if width < 32 or height < 16:  # below one PSMT4 block the file is linear
        return bytes(v for byte in data for v in (byte & 0xF, byte >> 4))
    upload_width, upload_height = _upload_shape4(width, height)
    upload_buffer = max(1, upload_width // 64)
    word_index = {}
    for y in range(upload_height):
        for x in range(upload_width):
            word_index[_address32(x, y, upload_buffer)] = y * upload_width + x
    read_buffer = max(2, width // 64)
    out = bytearray(width * height)
    for y in range(height):
        for x in range(width):
            nibble = _address4(x, y, read_buffer)
            byte = data[word_index[nibble >> 3] * 4 + ((nibble >> 1) & 3)]
            out[y * width + x] = byte >> 4 if nibble & 1 else byte & 0xF
    return bytes(out)


def read_pig(data: bytes) -> Image.Image:
    if data[:4] != MAGIC:
        raise ValueError("Not a P2IG image")
    swizzled = struct.unpack_from("<H", data, 0xC)[0] & SWIZZLED
    width_log, height_log, pixel_format = struct.unpack_from("<3H", data, 0x20)
    width, height = 1 << width_log, 1 << height_log
    at_palette, palette_size, at_pixels, _ = struct.unpack_from("<4I", data, 0x40)
    raw = data[at_palette : at_palette + palette_size]
    palette = _unswizzle_palette([raw[i : i + 4] for i in range(0, len(raw), 4)])
    rgba = []
    for entry in palette:
        r, g, b, a = entry
        rgba.append((r, g, b, min(255, a * 2)))
    if pixel_format == PSMT8:
        indices = data[at_pixels : at_pixels + width * height]
        if swizzled:
            indices = _unswizzle8(indices, width, height)
    elif pixel_format == PSMT4:
        packed = data[at_pixels : at_pixels + width * height // 2]
        if swizzled:
            indices = _unswizzle4(packed, width, height)
        else:
            indices = bytes(b >> shift & 0xF for b in packed for shift in (0, 4))
    else:
        raise ValueError(f"Unsupported P2IG pixel format {pixel_format:#x}")
    image = Image.new("RGBA", (width, height))
    image.putdata([rgba[i] for i in indices])
    return image
