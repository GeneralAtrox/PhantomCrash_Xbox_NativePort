"""S.L.A.I.'s text font: the FONT01..05 glyph pages and their metrics.

Glyph g lives on page g // 896 (FONT01.PIG ..) in a 32 x 28 grid of 16 x 18
pixel cells. The text renderer (0x207400, metrics 0x206B20) advances by a
per-glyph {left, width} pair: glyphs below 0xDF from the table at 0x36E2E0,
the rest from four fixed entries chosen by glyph range. The debug font
FONT_A01/02 (14 x 17 cells, 18 per row, from character 0x0E) is exported as
pictures only.
"""

from __future__ import annotations

import struct

FONT_FOLDER = "FONT"
FONT_PAGES = ("FONT01", "FONT02", "FONT03", "FONT04", "FONT05")
DEBUG_PAGES = ("FONT_A01", "FONT_A02")
GLYPHS_PER_PAGE, GLYPH_COLUMNS = 896, 32
GLYPH_WIDTH, GLYPH_HEIGHT = 16, 18
METRIC_TABLE, METRIC_TABLE_GLYPHS = 0x36E2E0, 0xDF
# Fixed {left, width} entries for glyphs past the table.
METRIC_WIDE, METRIC_NARROW, METRIC_SMALL, METRIC_211 = 0x36E4A0, 0x36E4A8, 0x36E4B0, 0x36E4B8
NARROW_RANGES = ((0x132, 0x13F), (0x14A, 0x14A), (0x163, 0x16C), (0x18D, 0x19F),
                 (0x221, 0x23F), (0x2ED, 0x2ED))
SMALL_GLYPHS = (0x1A9, 0x1AA)


def metric_entry(glyph):
    """Mirror of 0x206B20: the address of a glyph's {left, width} pair."""
    if glyph < METRIC_TABLE_GLYPHS:
        return METRIC_TABLE + 2 * glyph
    if glyph == 0x211:
        return METRIC_211
    if any(first <= glyph <= last for first, last in NARROW_RANGES):
        return METRIC_NARROW
    if glyph in SMALL_GLYPHS:
        return METRIC_SMALL
    return METRIC_WIDE


def glyph_metrics(program, overlay="towndvd.bin"):
    """{left, width} lists for every glyph on the five pages."""
    table = program.read(METRIC_TABLE, METRIC_211 + 2 - METRIC_TABLE, overlay)
    left, width = [], []
    for glyph in range(GLYPHS_PER_PAGE * len(FONT_PAGES)):
        at = metric_entry(glyph) - METRIC_TABLE
        pair = struct.unpack_from("<2B", table, at)
        left.append(pair[0])
        width.append(pair[1])
    return {"pages": [page.lower() + ".png" for page in FONT_PAGES],
            "glyphs_per_page": GLYPHS_PER_PAGE, "columns": GLYPH_COLUMNS,
            "cell": [GLYPH_WIDTH, GLYPH_HEIGHT], "left": left, "width": width}
