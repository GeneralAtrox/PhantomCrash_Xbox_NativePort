"""S.L.A.I.'s shop and dealer logos, for the shops the port draws in its own colours.

The port reuses Phantom Crash's Wild Machines, Sonic Amp and dealer screens for
S.L.A.I.'s shops (data/ui/shop_variants.json); a theme in data/ui/themes.json
pastes these logos into the recoloured atlases. The sheets are the
language-neutral ones: MOD (The Mechanist), CHI (Difference EG), CDS (Blood
Music), MAK02 (OMSK) and MAK04 (S&V Ma.FABRIK).
"""

from __future__ import annotations

from pathlib import Path

from .. import png
from .archive import SlaiDisc
from .pig import read_pig

SHOP_ART = ("MOD_LOGO01", "CHI_LOGO01", "CDS_LOGO01", "MAK02_1", "MAK04_1")
FOLDER = "textures/slai/shops"


def register_shop_art(image: Path, output: Path) -> list[str]:
    written = []
    with SlaiDisc(image) as disc:
        for name in SHOP_ART:
            found = disc.find(f"{name}.PIG")
            if not found:
                raise ValueError(f"S.L.A.I. shop art {name}.PIG is missing")
            picture = read_pig(disc.read(found[0]))
            relative = f"{FOLDER}/{name.lower()}.png"
            png.write_rgba(output / relative, picture.width, picture.height, picture.tobytes())
            written.append(relative)
    return written
