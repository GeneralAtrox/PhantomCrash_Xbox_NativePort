# Windows release

This is a work-in-progress Windows x64 port. The ZIP includes
`phantomcrash_port.exe`, the game data installer `phantomcrash_extract.exe`,
their libraries and editable `data`. Extract the entire ZIP before running it.
No game assets, executable, disc image or Xbox firmware are included; the
game is installed from your own European Xbox disc image. Python is not
needed.

## Setup

1. Use Windows 10 or newer with a Direct3D 11 capable GPU. Nothing else
   needs installing: the Microsoft Visual C++ runtime DLLs ship beside the
   EXE.
2. Run `Start Game.bat` or `phantomcrash_port.exe`. The first time, the game
   asks for your Phantom Crash (Europe) disc image and installs the game data
   from it in a console window (about ten seconds and 3.5 GB). Instead, you
   can drop the `.iso` onto `Install Game Data.bat` beforehand. Both trimmed
   XISO and full Redump-style images are supported.

The game data goes to `artifacts\europe` beside the EXE: `files` holds the
disc's files and `metadata` their sizes and SHA-256 hashes; `assets` holds
the textures, models, sounds and tables converted from them. The disc image
is only read during installation and can be moved or deleted afterwards.

## Updating to a new version

Extract the new ZIP and copy your `artifacts` and `mods` folders into it (or
extract the new ZIP over the old folder). The disc is not needed again:

- When a new version changes the converted assets, the game converts them
  again from `artifacts\europe\files` on its first start (a few seconds in
  a console window). `assets\manifest.json` records the converter version.
- The disc is asked for again only when files in `artifacts\europe\files`
  are missing or have changed size. `phantomcrash_extract.exe install
  --verify` also compares every file's hash; `--convert` converts again
  even when the assets are current; `phantomcrash_extract.exe status`
  reports what is installed.
- Installation writes to a staging folder that replaces the old one only
  when it is complete, so an interrupted run leaves the previous data in
  place and is redone on the next start.
- `mods`, `data`, settings and saves are never changed by installation.
  Keep personal asset changes in `mods`, which overlays `artifacts`, rather
  than editing the converted assets.

Settings and saves use the same app-data location as a source build. The
`--settings <file.json>` option selects a separate settings/save file.
Controllers, keyboard and mouse are supported; bindings are available under
Settings. F11 switches fullscreen. `Start Game.bat --start rumbling_d_rental`
opens the Class D rental shortcut; it uses and saves the current quest.

## Included changes

0.3.0 adds:

- **No prerequisites.** The Python extraction tools are replaced by
  `phantomcrash_extract.exe`, and the Visual C++ runtime DLLs ship beside the
  EXE. On first start the game asks for the disc image and installs the game
  data in about ten seconds; `Install Game Data.bat` does the same from a
  dropped `.iso`.
- **Updates keep your data.** A new version converts the assets again from
  the files already extracted when its converter changed, without the disc;
  installs are staged so an interrupted one leaves the old data in place.
  See "Updating to a new version" above.
- **Faster installation.** Extraction takes about 5 s instead of 27 s and
  conversion about 3 s, with parallel conversion, hardware SHA-256 and
  libdeflate PNG compression.
- The window title shows the release's version.

Existing users: copy your `artifacts` and `mods` folders into the new
version; the game converts the assets again on its first start. Python and
the separate Visual C++ runtime install are no longer needed.

0.2.0 added:

- **Opponent AI closer to the original.** Computer opponents take a
  computer rival only once they have locked it, turn on whoever just hurt
  them (leaders, champions and rankers always do), jump at a target that
  slips beside them and keep aiming after landing, and the attract demo's
  and RESULT's opponents fight at full skill. A provoked ranker fights in
  the common way for 8 s before choosing its own tactic again. Changes from
  the original are opt-in under `ai` in `data/world/rumbling.json`; see
  `docs/ORIGINAL_ROBOT_AI.md`.
- **Attract demo.** Left idle for 30 s, the title plays a RUMBLING battle
  for a minute, then the intro again; any button returns to the title.
- **Battle and RESULT cameras.** RESULT's backdrop watches a robot through
  the stage's fixed cameras, as the original does; the behind view follows
  the original's chase camera.
- **Cheats** (Tools > Cheats, all off by default): infinite money, armour
  and ammo, open contests, fill the empty hangars with one robot of each
  body model, add one of each part, CHIP and module, and go to any day.
  See `docs/CHEATS.md`.
- **Movement and animation.** Leg-record walk/dash speeds and turn rates,
  wheels that roll, strafing for every leg type, dash-turn leans and
  layered motion blends, and footsteps that follow the feet.
- **Fixes.** Robots no longer start under the Shinjuku street, light
  volume shadows have the original's length, and mixed-maker parts hang
  from the body's mounts when the `same_maker` rule is modded off.

The invincibility shield's original rainbow appearance is still unfinished;
see `docs/ENTRY_SHIELD_RENDERING.md`.

## Licenses and library sources

The port and the game data installer are MIT licensed (`LICENSE`). Bundled
third-party notices are in `licenses`. FFmpeg is dynamically linked under
LGPL 2.1 or later; its DLLs can be replaced with compatible rebuilt versions.
The release's companion `third-party-sources.zip` supplies the upstream
source archives, vcpkg patches/build recipes and FFmpeg configuration used
for these libraries. It is not needed to play.
The Microsoft Visual C++ runtime DLLs (`msvcp140*.dll`, `vcruntime140*.dll`)
are Microsoft's redistributable files, shipped under the Visual Studio
license terms (`licenses/microsoft-visual-cpp-runtime.txt`).

## Reproducing a package

From the matching source tag, run `scripts/build-port.ps1`, then
`python tools/package_release.py` (`--output DIR` writes elsewhere). Packaging
uses `build/port`, the Visual Studio C++ Redist for the runtime DLLs the
binaries import, and the pinned `.tools/vcpkg` sources, and writes the two
ZIPs plus SHA-256 checksums to `build/releases`. Only the port, data, tooling
and library files on its allowlist are included; extracted game content is
excluded.
