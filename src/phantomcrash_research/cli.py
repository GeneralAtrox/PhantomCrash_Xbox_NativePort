"""Command-line entry point: ``python -m phantomcrash_research <command>``."""

from __future__ import annotations

import argparse
import json
from pathlib import Path
import sys

from . import stats_csv, xbe
from .convert import convert, update_robots, update_town
from .extract import extract
from .mechassault import arm_swap as mechassault_arm
from .mechassault import export as mechassault_export
from .xdvdfs import XdvdfsImage

REPOSITORY_ROOT = Path(__file__).resolve().parents[2]
DEFAULT_DISC = REPOSITORY_ROOT.parent / "Games" / "Phantom Crash (Europe).xiso.iso"
DEFAULT_OUTPUT = REPOSITORY_ROOT / "artifacts" / "europe"
DEFAULT_ASSETS = DEFAULT_OUTPUT / "assets"
DEFAULT_MODS = REPOSITORY_ROOT / "mods"


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
        f"Converted {textures} textures, {len(manifest['models'])} models, {effects} effects, "
        f"{tables['sprites']} sprites and "
        f"{tables['resources']} resources to {arguments.output}"
    )
    return 0


def _command_convert_robots(arguments: argparse.Namespace) -> int:
    report = None if arguments.quiet else print
    robots = update_robots(Path(arguments.files), Path(arguments.output), report)
    print(
        f"Converted {len(robots['models'])} robot models, {robots['textures']} paints and "
        f"{robots['motion_banks']} motion banks to {arguments.output}"
    )
    return 0


def _command_convert_mechassault_vfx(arguments: argparse.Namespace) -> int:
    summary = mechassault_export.convert(Path(arguments.disc), Path(arguments.output))
    print(
        f"Converted {summary['effects']} weapon effects, {summary['meshes']} meshes and "
        f"{summary['textures']} textures to {arguments.output}"
    )
    for label in ("skipped", "missing"):
        if summary[label]:
            print(f"{label}: {len(summary[label])}, e.g. {summary[label][:3]}")
    return 0


def _command_swap_mechassault_arm(arguments: argparse.Namespace) -> int:
    for model, source in ((arguments.right, arguments.right_arm), (arguments.left, arguments.left_arm)):
        if not model:
            continue
        directory, name = source.rsplit("\\", 1)
        summary = mechassault_arm.swap_arm(
            Path(arguments.disc),
            arguments.archive,
            directory,
            name,
            Path(arguments.assets) / model,
            Path(arguments.output),
            model,
        )
        print(f"Wrote {arguments.output}/{model}: {len(summary['parts'])} parts, scale {summary['scale']}")
    return 0


def _command_export_stats(arguments: argparse.Namespace) -> int:
    counts = stats_csv.export(Path(arguments.assets), Path(arguments.output))
    print(f"Wrote {len(counts)} tables ({sum(counts.values())} rows) to {arguments.output}")
    return 0


def _command_convert_town(arguments: argparse.Namespace) -> int:
    town = update_town(Path(arguments.files), Path(arguments.output))
    print(
        f"Converted {len(town['contests'])} contests, {len(town['classes'])} classes and "
        f"{len(town['calendar'])} calendar days to {arguments.output}"
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

    robot_conversion = commands.add_parser(
        "convert-robots", help="Redo only the robot part of an existing conversion"
    )
    robot_conversion.add_argument("--files", default=str(DEFAULT_OUTPUT / "files"))
    robot_conversion.add_argument("--output", default=str(DEFAULT_ASSETS))
    robot_conversion.add_argument("--quiet", action="store_true", help="Only print the summary")
    robot_conversion.set_defaults(handler=_command_convert_robots)

    town_conversion = commands.add_parser(
        "convert-town", help="Redo only the town's tables (tables/town.json)"
    )
    town_conversion.add_argument("--files", default=str(DEFAULT_OUTPUT / "files"))
    town_conversion.add_argument("--output", default=str(DEFAULT_ASSETS))
    town_conversion.set_defaults(handler=_command_convert_town)

    mechassault = commands.add_parser(
        "convert-mechassault-vfx",
        help="Convert a MechAssault disc's weapon effects into a mod for the port",
    )
    mechassault.add_argument("disc", help="MechAssault .iso or .xiso")
    mechassault.add_argument("--output", default=str(DEFAULT_MODS))
    mechassault.set_defaults(handler=_command_convert_mechassault_vfx)

    arm = commands.add_parser(
        "swap-mechassault-arm",
        help="Give Phantom Crash arm models a MechAssault mech's gun arms (a mod)",
    )
    arm.add_argument("disc", help="MechAssault .iso or .xiso")
    arm.add_argument("--archive", default="Mission06a.mgf")
    madcat = r"\Models\Mechs\Heavy\MadCat\Model_Low_Eny"
    arm.add_argument("--right-arm", default=madcat + r"\mad_rgimparm\mad_rgimparm")
    arm.add_argument("--left-arm", default=madcat + r"\mad_lgimparm\mad_lgimparm")
    arm.add_argument("--right", default="models/binlink/105.gltf", help="right arm model to replace")
    arm.add_argument("--left", default="models/binlink/92.gltf", help="left arm model to replace")
    arm.add_argument("--assets", default=str(DEFAULT_ASSETS))
    arm.add_argument("--output", default=str(DEFAULT_MODS))
    arm.set_defaults(handler=_command_swap_mechassault_arm)

    stats_export = commands.add_parser(
        "export-stats", help="Write every part, item and opponent's stats as CSV files"
    )
    stats_export.add_argument("--assets", default=str(DEFAULT_ASSETS))
    stats_export.add_argument("--output", default=str(DEFAULT_OUTPUT / "stats"))
    stats_export.set_defaults(handler=_command_export_stats)
    return parser


def main(argv: list[str] | None = None) -> int:
    arguments = build_parser().parse_args(argv)
    return arguments.handler(arguments)


if __name__ == "__main__":
    sys.exit(main())
