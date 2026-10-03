# Windows release

This is a work-in-progress Windows x64 port. The ZIP includes
`phantomcrash_port.exe`, its game libraries, editable `data`, and the disc
extraction tools. Extract the entire ZIP before running it.

## Setup

1. Use Windows 10 or newer with a Direct3D 11 capable GPU. Install the
   [Microsoft Visual C++ x64 runtime](https://aka.ms/vs/17/release/vc_redist.x64.exe)
   if it is not already installed.
2. Install Python 3.11 or newer to extract your own European Xbox disc image.
   Open a terminal in the extracted release folder and run:

   ```powershell
   python .\tools\phantomcrash.py extract --disc "C:\Games\Phantom Crash (Europe).xiso.iso"
   python .\tools\phantomcrash.py convert
   ```

   Both trimmed XISO and full Redump-style images are supported. Extraction
   creates `artifacts\europe\files`; conversion creates
   `artifacts\europe\assets`. No game assets, executable, disc image or Xbox
   firmware are included in this download.
3. Run `Start Game.bat` or `phantomcrash_port.exe`.

If you already extracted the game, copy your `artifacts` folder beside the
EXE. Rerun `python .\tools\phantomcrash.py convert --replace` using the new
tools so the stage props, monitor surfaces and effect resources are current.
This replaces converted assets; keep personal asset changes in `mods`.

Settings and saves use the same app-data location as a source build. The
`--settings <file.json>` option selects a separate settings/save file.
Controllers, keyboard and mouse are supported; bindings are available under
Settings. F11 switches fullscreen. `Start Game.bat --start rumbling_d_rental`
opens the Class D rental shortcut; it uses and saves the current quest.

## Included changes

0.2.0 adds:

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

The port and extraction tools are MIT licensed (`LICENSE`). Bundled
third-party notices are in `licenses`. FFmpeg is dynamically linked under
LGPL 2.1 or later; its DLLs can be replaced with compatible rebuilt versions.
The release's companion `third-party-sources.zip` supplies the upstream
source archives, vcpkg patches/build recipes and FFmpeg configuration used
for these libraries. It is not needed to play.

## Reproducing a package

From the matching source tag, run `scripts/build-port.ps1`, then
`python tools/package_release.py`. Packaging uses `build/port` and the
pinned `.tools/vcpkg` sources, and writes the two ZIPs plus SHA-256 checksums
to `build/releases`. Only the port, data, tooling and library files on its
allowlist are included; extracted game content is excluded.
