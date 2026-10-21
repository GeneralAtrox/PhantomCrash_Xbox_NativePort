"""Convert S.L.A.I.'s Club Wired city sheets for the port's Rumble S.L.A.I.

Each city's sheet ``UK_WIR_<CITY>01.PIG`` (512x512, English package; its
layout is ``UK_WIR_CI_<CITY>.AST``) holds the city's emblem, the venue's name
and the "<CITY> / NIGHT" and "<CITY> / DAY" labels the screen draws
(data/ui/slai_rumble.json, city_art). The sheets are written whole as PNGs to
textures/slai/wired/<city>.png in the S.L.A.I. asset root; the rectangles
that pick the pieces out are in the data file.

Installs made before this step lack the sheets. ``main`` adds them to an
existing install without converting the rest again.
"""

from pathlib import Path
import argparse

from .archive import SlaiDisc
from .pig import read_pig

ART_DIR = "textures/slai/wired"
# By the table's city column (campaign.CITIES): file stem -> disc name.
CITY_SHEETS = (
    ("tokyo", "TOKYO"),
    ("stuttgart", "STUTTGART"),
    ("new_york", "NEWYORK"),
    ("london", "LONDON"),
    ("hong_kong", "HONGKONG"),
    ("cairo", "CAIRO"),
    ("las_vegas", "LASVEGAS"),
)


def register_wired_art(image: Path, output: Path) -> dict:
    """Write each city's Wired sheet into the S.L.A.I. asset root `output`."""
    folder = output / ART_DIR
    folder.mkdir(parents=True, exist_ok=True)
    with SlaiDisc(image) as disc:
        for stem, name in CITY_SHEETS:
            source = f"UK_WIR_{name}01.PIG"
            hits = disc.find(source)
            if not hits:
                raise ValueError("Missing " + source)
            read_pig(disc.read(hits[0])).save(folder / f"{stem}.png")
    return {"wired_city_sheets": len(CITY_SHEETS)}


def main() -> int:
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--root", type=Path, required=True)
    parser.add_argument("--disc", type=Path, required=True)
    args = parser.parse_args()
    assets = args.root.resolve() / "artifacts/slai/assets"
    if not (assets / "installed.json").is_file():
        print("S.L.A.I. is not installed under " + str(args.root))
        return 1
    print(register_wired_art(args.disc.resolve(), assets))
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
