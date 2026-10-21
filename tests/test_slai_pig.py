"""P2IG decoding checks independent of disc-derived files."""

import struct
import unittest

from phantomcrash_research.slai.pig import (
    PSMT4,
    _address4,
    _address32,
    _upload_shape4,
    read_pig,
)

FLAGS_SWIZZLED = 0xE  # the disc's usual flags: 0x8 set, stored through the GS layout


def swizzle4(indices: list[int], width: int, height: int) -> bytes:
    """Lay linear 4-bit indices out as the game's PSMCT32 upload."""
    upload_width, upload_height = _upload_shape4(width, height)
    upload_buffer = max(1, upload_width // 64)
    word_index = {
        _address32(x, y, upload_buffer): y * upload_width + x
        for y in range(upload_height)
        for x in range(upload_width)
    }
    out = bytearray(width * height // 2)
    for y in range(height):
        for x in range(width):
            nibble = _address4(x, y, max(2, width // 64))
            at = word_index[nibble >> 3] * 4 + ((nibble >> 1) & 3)
            out[at] |= indices[y * width + x] << (4 if nibble & 1 else 0)
    return bytes(out)


def pig4(width: int, height: int, indices: list[int]) -> bytes:
    header = bytearray(0x50)
    header[:4] = b"P2IG"
    struct.pack_into("<3H", header, 0x20, width.bit_length() - 1, height.bit_length() - 1, PSMT4)
    if width >= 32 and height >= 16:
        struct.pack_into("<H", header, 0xC, FLAGS_SWIZZLED)
    palette = b"".join(bytes((i * 16, 255 - i * 16, i, 0x80)) for i in range(16))
    pixels = swizzle4(indices, width, height) if width >= 32 and height >= 16 else bytes(width * height // 2)
    struct.pack_into("<4I", header, 0x40, 0x50, len(palette), 0x50 + len(palette), len(pixels))
    return bytes(header) + palette + pixels


class Pig4Test(unittest.TestCase):
    def test_psmt4_round_trips_through_the_gs_layout(self) -> None:
        for width, height in ((512, 128), (256, 512), (64, 64), (128, 16), (32, 16)):
            indices = [(x // 3 + y * 7 + (x * y) % 5) & 0xF for y in range(height) for x in range(width)]
            image = read_pig(pig4(width, height, indices))
            self.assertEqual(image.size, (width, height))
            decoded = [r // 16 for r, *_ in image.getdata()]
            self.assertEqual(decoded, indices, (width, height))

    def test_textures_below_one_block_are_linear(self) -> None:
        indices = [(i * 5) & 0xF for i in range(64)]
        packed = bytes(indices[i] | indices[i + 1] << 4 for i in range(0, 64, 2))
        data = bytearray(pig4(8, 8, [0] * 64))
        data[-32:] = packed
        self.assertEqual([r // 16 for r, *_ in read_pig(bytes(data)).getdata()], indices)

    def test_upload_addresses_cover_the_texture_once(self) -> None:
        width, height = 512, 128
        upload_width, upload_height = _upload_shape4(width, height)
        words = {_address32(x, y, 1) for y in range(upload_height) for x in range(upload_width)}
        nibbles = {_address4(x, y, width // 64) >> 3 for y in range(height) for x in range(width)}
        self.assertEqual(words, nibbles)


if __name__ == "__main__":
    unittest.main()
