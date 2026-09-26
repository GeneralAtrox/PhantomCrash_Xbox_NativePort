"""Command-line entry point: ``python -m phantomcrash_research <command>``."""

from __future__ import annotations

import argparse
import json
from pathlib import Path
import sys

from . import xbe
from .convert import convert
from .extract import extract
from .xdvdfs import XdvdfsImage

REPOSITORY_ROOT = Path(__file__).resolve().parents[2]
DEFAULT_DISC = REPOSITORY_ROOT.parent / "Games" / "Phantom Crash (Europe).xiso.iso"
DEFAULT_OUTPUT = REPOSITORY_ROOT / "artifacts" / "europe"
DEFAULT_ASSETS = DEFAULT_OUTPUT / "assets"


def _command_list(arguments: argparse.Namespace) -> int:
    with XdvdfsImage(arguments.disc) as image:
        for entry in image.files():
            print(f"{entry.size:>12,}  {entry.path}")
    return 0


def _command_xbe(arguments: argparse.Namespace) -> int:
    if arguments.file:
        data = Path(arguments.file).read_bytes()
    else:
        with XdvdfsImage(arguments.disc) as image:
            data = image.read(image.find("default.xbe"))
    print(json.dumps(xbe.parse(data).to_dict(), indent=2))
    return 0


def _command_extract(arguments: argparse.Namespace) -> int:
    report = None if arguments.quiet else print
    result = extract(Path(arguments.disc), Path(arguments.output), report)
    header = result["xbe"]
    print(
        f"Extracted {result['disc']['file_count']} files "
        f"({result['disc']['total_bytes']:,} bytes) to {arguments.output}\n"
        f"XBE {header['title_code']} '{header['title_name']}' XDK {header['xdk_build']} "
        f"sha256 {header['sha256']}"
    )
    return 0


def _command_convert(arguments: argparse.Namespace) -> int:
    report = None if arguments.quiet else print
    manifest = convert(Path(arguments.files), Path(arguments.output), arguments.replace, report)
    textures = sum(len(pack["textures"]) for pack in manifest["texture_packs"])
    effects = sum(len(bank["effects"]) for bank in manifest["sound_banks"])
    tables = manifest["tables"]
    print(
        f"Converted {textures} textures, {effects} effects, {tables['sprites']} sprites and "
        f"{tables['resources']} resources to {arguments.output}"
    )
    return 0


def build_parser() -> argparse.ArgumentParser:
    parser = argparse.ArgumentParser(prog="phantomcrash", description=__doc__)
    commands = parser.add_subparsers(dest="command", required=True)

    listing = commands.add_parser("list", help="List every file on the disc")
    listing.add_argument("--disc", default=str(DEFAULT_DISC))
    listing.set_defaults(handler=_command_list)

    header = commands.add_parser("xbe", help="Print the XBE header as JSON")
    header.add_argument("--disc", default=str(DEFAULT_DISC))
    header.add_argument("--file", help="Read an extracted XBE instead of the disc")
    header.set_defaults(handler=_command_xbe)

    extraction = commands.add_parser("extract", help="Extract the disc into artifacts")
    extraction.add_argument("--disc", default=str(DEFAULT_DISC))
    extraction.add_argument("--output", default=str(DEFAULT_OUTPUT))
    extraction.add_argument("--quiet", action="store_true", help="Only print the summary")
    extraction.set_defaults(handler=_command_extract)

    conversion = commands.add_parser(
        "convert", help="Convert extracted textures, effects and tables to open formats"
    )
    conversion.add_argument("--files", default=str(DEFAULT_OUTPUT / "files"))
    conversion.add_argument("--output", default=str(DEFAULT_ASSETS))
    conversion.add_argument(
        "--replace", action="store_true", help="Delete and rebuild an earlier conversion"
    )
    conversion.add_argument("--quiet", action="store_true", help="Only print the summary")
    conversion.set_defaults(handler=_command_convert)
    return parser


def main(argv: list[str] | None = None) -> int:
    arguments = build_parser().parse_args(argv)
    return arguments.handler(arguments)


if __name__ == "__main__":
    sys.exit(main())
