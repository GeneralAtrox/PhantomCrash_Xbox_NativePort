"""Convert S.L.A.I.'s talk scenes into the open files the port's scene player loads.

Writes into the S.L.A.I. asset root:

- ``talk/<folder>/*.png`` and ``*.json``: portraits, expressions, the talk
  window art (``talk/common``), the Knocker (``talk/de000``) and the news and
  event pictures (``talk/news/<n>``), as PNG plus ANB animation JSON;
- ``2d/s_chara/<group>/*.png`` and ``2d/s_chip/*.png``: small icons;
- ``font/font01..05.png`` and ``font/glyphs.json``: the glyph pages and metrics;
- ``talk/event/<language>/<script>.json``: every EVT script per language;
- ``tables/slai_talk.json``: opcodes, characters, story events and colours;
- ``slai/assets/sound/cue_NNN.wav``: the sound cues the talk scenes play.
"""

from __future__ import annotations

import json
import wave
from pathlib import Path

from .anb import read_anb
from .archive import SlaiDisc
from .assets import save
from .pig import read_pig
from .program import ProgramData
from .talk_font import DEBUG_PAGES, FONT_FOLDER, FONT_PAGES, glyph_metrics
from .talk_tables import (AMBIENT_COUNT, TALK_LANGUAGES, evt_json, package_directory,
                          talk_table)

TABLE_FILE = "tables/slai_talk.json"
FONT_FILE = "font/glyphs.json"
EVENT_ROOT = "talk/event"
PICTURE_FOLDERS = ("TALK/", "2D/S_CHARA/", "2D/S_CHIP")
EVENT_FOLDERS = ("TALK/EVENT",)
SKIPPED_FILES = ("FILELIST.LST",)
# Sound cues the talk VM plays itself: the typing blip, news frame, panel
# open/close, picture change and expression change (0x26E380, 0x26D0F0,
# 0x26C670, 0x26C850, 0x26D5C0, 0x26F510).
TALK_CUES = (0x0A, 0x1F, 0x2A, 0x2B, 0x2C, 0x2D)
SE_OPCODE = "se"


def _pictures(disc, packages, folder, files, output):
    """PIG -> PNG and ANB -> JSON for one shipped folder."""
    destination = output / folder.lower()
    destination.mkdir(parents=True, exist_ok=True)
    pictures = animations = 0
    for file in packages.get(files["offset"], []):
        stem, _, kind = file.name.lower().rpartition(".")
        if kind == "pig":
            read_pig(disc.read(file)).save(destination / (stem + ".png"))
            pictures += 1
        elif kind == "anb":
            save(destination / (stem + ".json"), read_anb(disc.read(file)))
            animations += 1
    return pictures, animations


def _scripts(disc, packages, folders, output):
    scripts = {}
    for language, folder in TALK_LANGUAGES.items():
        destination = output / EVENT_ROOT / language
        destination.mkdir(parents=True, exist_ok=True)
        for file in packages.get(folders[folder]["offset"], []):
            if file.name in SKIPPED_FILES:
                continue
            name = file.name.lower().rpartition(".")[0]
            script = evt_json(disc.read(file))
            save(destination / (name + ".json"), script)
            scripts.setdefault(name, script)
    return scripts


def _cues(table, scripts):
    wanted = set(TALK_CUES) | {cue for cue in table["ambient_sounds"][1:AMBIENT_COUNT] if cue}
    for script in scripts.values():
        wanted |= {line[1] for line in script["code"] if line[0] == SE_OPCODE and line[1] >= 0}
    return sorted(wanted)


def _render_cues(program, cue_ids, output, loops=()):
    """Render the talk cues like register_sound does; missing banks skip it.
    Cues in `loops` (the ambient loops) are one pass of their loop region."""
    from .sound_cues import recover_cues
    from .sound_envelope import RATE
    from .sound_render import render_sequence

    root = output / "slai/assets/sound"
    if not (root / "banks.json").is_file():
        return 0, list(cue_ids)
    banks = json.loads((root / "banks.json").read_text(encoding="utf-8"))["banks"]
    cues = recover_cues(program, banks)["cues"]
    rendered, pending = 0, []
    for cue_id in cue_ids:
        cue = cues[cue_id] if cue_id < len(cues) else None
        if not cue:
            pending.append(cue_id)
            continue
        bank = banks[cue["bank"]]
        try:
            pcm = render_sequence(bank, bank["sequences"][cue["sequence"]], root,
                                  loop=cue_id in loops)
        except ValueError:
            pending.append(cue_id)
            continue
        with wave.open(str(root / f"cue_{cue_id:03d}.wav"), "wb") as writer:
            writer.setparams((2, 2, RATE, 0, "NONE", "not compressed"))
            writer.writeframes(pcm.tobytes())
        rendered += 1
    return rendered, pending


def register_talk(image: Path, output: Path):
    """Write the talk scenes' pictures, animations, font, scripts and table."""
    with SlaiDisc(image) as disc:
        program = ProgramData(disc)
        folders = package_directory(program)
        packages = disc.packages()
        pictures = animations = 0
        for folder, files in folders.items():
            if folder.startswith(PICTURE_FOLDERS) and not folder.startswith(EVENT_FOLDERS):
                counts = _pictures(disc, packages, folder, files, output)
                pictures, animations = pictures + counts[0], animations + counts[1]
        font = {f.name.rpartition(".")[0]: f
                for f in packages.get(folders[FONT_FOLDER]["offset"], [])}
        (output / "font").mkdir(parents=True, exist_ok=True)
        for page in FONT_PAGES + DEBUG_PAGES:
            read_pig(disc.read(font[page])).save(output / "font" / (page.lower() + ".png"))
        save(output / FONT_FILE, glyph_metrics(program))
        scripts = _scripts(disc, packages, folders, output)
        table = talk_table(program)
        table["sound_cues"] = _cues(table, scripts)
        rendered, pending = _render_cues(program, table["sound_cues"], output,
                                         set(table["ambient_sounds"]) - {0})
        table["pending_sound_cues"] = pending
        save(output / TABLE_FILE, table)
    return {"talk_pictures": pictures, "talk_animations": animations,
            "talk_scripts": len(scripts), "talk_events": len(table["events"]),
            "talk_sound_cues": rendered}
