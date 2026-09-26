"""Convert extracted disc files into the open formats the port loads.

Output (``artifacts/europe/assets`` by default):

- ``textures/<pack>/<index>.png``: every texture of every ``*_tex.bin`` pack.
- ``sound/sfx/<bank>/<index>.wav``: every effect of every SE BIN bank, as PCM.
- ``tables/*.json``: resources, sprites, music (with credits and the volume
  curve), movie and system font tables from the XBE.
- ``manifest.json``: what was converted from which input.

Movies (``.sfd``) and music (PCM ``.wav``) are already open formats; the port
reads them from the extracted ``files/Media`` tree, so they are not copied.
"""

from __future__ import annotations

from dataclasses import asdict
import json
from pathlib import Path
import shutil
from typing import Callable

from . import audio, game_tables, png, texture_pack

MANIFEST = "manifest.json"
FORMAT_VERSION = 1
TEXTURE_DIRECTORIES = ("scroll", "texture")


def _write_json(path: Path, data: object) -> None:
    path.parent.mkdir(parents=True, exist_ok=True)
    path.write_text(json.dumps(data, indent=1) + "\n", encoding="utf-8")


def _prepare_output(output: Path, replace: bool) -> None:
    if output.exists() and any(output.iterdir()):
        if not replace:
            raise FileExistsError(f"Output directory is not empty: {output} (use --replace)")
        if not (output / MANIFEST).is_file():
            raise FileExistsError(f"Refusing to replace {output}: it has no {MANIFEST}")
        shutil.rmtree(output)
    output.mkdir(parents=True, exist_ok=True)


def _pack_name(relative: str) -> str:
    """``scroll/logo_tex.bin`` -> ``scroll/logo_tex``."""
    return relative.rsplit(".", 1)[0]


def convert_textures(media: Path, output: Path, report: Callable[[str], None]) -> list[dict]:
    records = []
    for directory in TEXTURE_DIRECTORIES:
        for source in sorted((media / directory).glob("*.bin")):
            relative = f"{directory}/{source.name}"
            data = source.read_bytes()
            textures = []
            for texture in texture_pack.parse(data):
                fmt = texture.format
                target = output / "textures" / _pack_name(relative) / f"{texture.index}.png"
                png.write_rgba(target, fmt.width, fmt.height, texture.rgba(data))
                textures.append(
                    {
                        "index": texture.index,
                        "format": fmt.name,
                        "width": fmt.width,
                        "height": fmt.height,
                        "levels": fmt.levels,
                    }
                )
            records.append({"pack": relative, "textures": textures})
            report(f"{len(textures):>3} textures  {relative}")
    return records


def convert_sound_banks(media: Path, output: Path, report: Callable[[str], None]) -> list[dict]:
    records = []
    for source in sorted((media / "sound").glob("*.bin")):
        data = source.read_bytes()
        if not data.startswith(audio.BANK_MAGIC):
            continue
        bank = source.stem
        effects = []
        for entry in audio.parse_bank(data):
            audio.write_pcm16_wav(output / "sound" / "sfx" / bank / f"{entry.index:02d}.wav", entry.wave)
            wave = entry.wave
            effects.append(
                {
                    "index": entry.index,
                    "flags": entry.flags,
                    "slot": entry.slot,
                    "parameters": entry.parameters,
                    "source_format": wave.format_tag,
                    "channels": wave.channels,
                    "sample_rate": wave.sample_rate,
                }
            )
        records.append({"bank": f"sound/{source.name}", "effects": effects})
        report(f"{len(effects):>3} effects   sound/{source.name}")
    return records


def _slot_sources(tables: game_tables.GameTables) -> dict[str, list[dict]]:
    """Texture slot -> every disc pack texture that can fill it (usually one)."""
    sources: dict[str, list[dict]] = {}
    for resource in tables.resources:
        if resource.path is None or resource.first_slot is None:
            continue
        for index in range(resource.slot_count):
            entry = {"pack": _pack_name(resource.path), "texture": index}
            sources.setdefault(str(resource.first_slot + index), []).append(entry)
    return sources


def write_tables(executable: Path, output: Path) -> dict:
    tables = game_tables.load(executable.read_bytes())
    directory = output / "tables"
    _write_json(directory / "resources.json", [asdict(resource) for resource in tables.resources])
    _write_json(
        directory / "sprites.json",
        {
            "slots": _slot_sources(tables),
            "sprites": [asdict(sprite) for sprite in tables.sprites],
        },
    )
    _write_json(
        directory / "music.json",
        {
            "tracks": tables.music,
            "scene_music": tables.scene_music,
            "credits": [
                {"file": f"sound/{tables.music[credit.track]}", **asdict(credit)}
                for credit in tables.music_credits
                if credit.track < len(tables.music)
            ],
            "volume_millibels": tables.volume_millibels,
        },
    )
    _write_json(
        directory / "font.json",
        {
            "image": "textures/scroll/ascii_font_sys_tex/0.png",
            "cell": 18,
            "columns": 28,
            "characters": tables.font_characters,
            "metrics": [list(metric) for metric in tables.font_metrics],
        },
    )
    _write_json(
        directory / "movies.json",
        {
            "movies": tables.movies,
            "binlink_packs": tables.binlink_packs,
        },
    )
    return {
        "resources": len(tables.resources),
        "sprites": len(tables.sprites),
        "music": len(tables.music),
        "movies": len(tables.movies),
        "font_glyphs": len(tables.font_characters),
    }


def convert(
    files: Path,
    output: Path,
    replace: bool = False,
    progress: Callable[[str], None] | None = None,
) -> dict:
    """Convert ``files`` (an extraction's ``files`` directory) into ``output``."""
    report = progress or (lambda _message: None)
    media = files / "Media"
    executable = files / "default.xbe"
    if not media.is_dir() or not executable.is_file():
        raise FileNotFoundError(f"{files} does not look like an extracted disc (Media, default.xbe)")
    _prepare_output(output, replace)
    manifest = {"format_version": FORMAT_VERSION, "source": str(files), "complete": False}
    # Written first so an interrupted run can still be replaced.
    _write_json(output / MANIFEST, manifest)
    manifest["tables"] = write_tables(executable, output)
    manifest["texture_packs"] = convert_textures(media, output, report)
    manifest["sound_banks"] = convert_sound_banks(media, output, report)
    manifest["complete"] = True
    _write_json(output / MANIFEST, manifest)
    return manifest
