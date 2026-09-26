# Phantom Crash — Original Xbox modern port

Research tooling and a clean-room modern C++ port of *Phantom Crash* for a
user-supplied Original Xbox disc image. This repository does **not** contain
the game, its executable, extracted assets, or any Xbox firmware.

The workflow follows the Record of Lodoss War project: extract and study the
user's disc, understand behaviour with Ghidra and the
[xemu_decomp](https://github.com/GeneralAtrox/xemu_decomp) emulator fork,
then reimplement it as a native SDL3 + Direct3D 11 program with modern
resolution, widescreen and frame-rate support.

## Workspace

```text
C:\Projects\Xemu\
├── Games\            # user-supplied disc images (outside every repository)
├── PhantomCrash\     # this repository
└── xemu_decomp\      # emulator fork used for runtime research
```

Inside this repository, `artifacts\`, `research-output\`, `build\` and
`.tools\` are generated and ignored by Git.

## Requirements

- Python 3.11+ (no third-party runtime dependencies; `pytest` for tests)
- PowerShell 7.4+
- Visual Studio 2022 with the C++ workload, CMake 3.25+
- For static analysis: Ghidra 12.1.2 and Java 21 (see
  [the Ghidra tooling notes](docs/GHIDRA_TOOLING.md))

## Extract the disc

```powershell
.\scripts\extract-game.ps1
# or
python .\tools\phantomcrash.py extract
python .\tools\phantomcrash.py list
python .\tools\phantomcrash.py xbe
python .\tools\phantomcrash.py convert
```

The default disc is `..\Games\Phantom Crash (Europe).xiso.iso`; pass `--disc`
for another image. Both trimmed `.xiso` images and full Redump-style images are
supported. Output goes to `artifacts\europe`:

- `files\`: every file on the disc;
- `metadata\disc.json`: image hash, partition offset, and per-file sector,
  size and SHA-256;
- `metadata\xbe.json`: the parsed `default.xbe` header, certificate, SDK
  library versions and section table.

Extraction refuses a non-empty output directory so runs never mix.

`convert` turns the extracted files into formats the port reads, in
`artifacts\europe\assets` (about a minute; the script runs it after
extraction):

- `textures\<folder>\<pack>\<n>.png`: every Xbox texture pack, unswizzled;
- `sound\sfx\<bank>\<nn>.wav`: every sound-effect bank entry as 16-bit PCM;
- `tables\*.json`: the resource, sprite, scene-music and movie tables read
  from `default.xbe`.

Movies (`.sfd`) and music (`.wav`) are already open formats and stay in
`files\Media`. `convert` also refuses a non-empty output; pass `--replace` to
redo a previous conversion.

## Data and mods

The port's behaviour lives in editable JSON under `data\` (intro sequence,
title, main menu, music and effect names). Every file path in it resolves
against `mods\`, then `artifacts\europe\assets`, then
`artifacts\europe\files\Media`, so a file placed in `mods\` under the same
relative path replaces the original.

## Static analysis

```powershell
pwsh -NoProfile -File .\scripts\setup-ghidra-tools.ps1
pwsh -NoProfile -File .\scripts\build-ghidra-xbe.ps1 -Install
pwsh -NoProfile -File .\scripts\analyze-ghidra.ps1
```

See [the Ghidra tooling notes](docs/GHIDRA_TOOLING.md) and
[the European baseline](docs/EUROPE_BASELINE.md).

## Build the port

```powershell
pwsh -NoProfile -File .\scripts\build-port.ps1 -Test
.\launch.bat
```

`launch.bat` works like the Lodoss `Start Game.bat`. It rebuilds anything
out of date, puts the vcpkg DLL folders on PATH, and runs the port from the
project folder with `--root`, passing any extra arguments through. It pauses to
show the error if the build fails or the game exits with an error. Run
`build-port.ps1` once first to configure the build.

The port finds `data\` by searching upward from the working directory and the
executable, or takes `--root <folder>`. Other options: `--fullscreen`,
`--windowed`, `--size <width>x<height>`, `--start <screen>` to begin on
`title`, `main_menu`, `option`, `sound_config`, `button_config`,
`screen_adjust` or a placeholder instead of the intro, and
`--smoke-test [frames] [--capture <file.bmp>] [--press <frame>:<button>]` for
a hidden, bounded run that can press buttons and save its last frame. A smoke
run steps one 60 Hz logic tick per frame, so `--press` frames are game ticks.

The port boots into the intro from `data\boot\intro.json`: Phantagram,
Genki, the Sofdec/ADX screen and Dolby (first boot only), then an attract
movie. Any button skips the current step; one tap skips one step. Movies are
read from the extracted `Media\movie` folder, or from `mods\` first.

After the intro comes the title screen (press START) and the main menu, with
the title music and menu sounds. QUEST MODE opens NEW GAME / LOAD GAME; the
modes themselves are not ported yet, so they show a placeholder that returns
to the menu. VS MODE needs two controllers (the keyboard counts as one). The
layouts, sounds and timings are in `data\ui\title.json`,
`data\ui\main_menu.json` and `data\audio\`.

OPTION opens the option screen with its scrolling help line and the credit
for the current music. VIBRATION asks YES/NO and is saved with the settings
(nothing rumbles yet); DEFAULT restores the defaults; B moves to EXIT. The
screen is in `data\ui\option.json` and the message box in
`data\ui\message_box.json`. SOUND CONFIG sets the music and effect volumes
(heard at once, through the game's volume table in the converted
`tables\music.json`) and plays test effects; BUTTON CONFIG sets the control
type, view, beginner mode and the button for each action; SCREEN ADJUST
moves the whole picture with the D-pad (X centres it). They are
`data\ui\sound_config.json`, `button_config.json` and `screen_adjust.json`,
and their settings are saved.
Text uses the game's own font from the converted `tables\font.json`; after
updating the converter, run `python .\tools\phantomcrash.py convert
--replace` again to get it.

Controls: keyboard Enter = START, Z/Space = A, X/Backspace = B, Esc = BACK,
arrows = D-pad; gamepads map by position (south = A). F11 or Alt+Enter toggles
fullscreen. The window's Settings menu has VSync, a frame-rate cap from 30 to
144 FPS (VSync off with no cap is unlimited), a frame counter and Pause When
Unfocused; choices are saved in `%APPDATA%\PhantomCrash\PhantomCrash Port`.

vcpkg is checked out under `.tools\vcpkg` at the same baseline as Lodoss, so
packages already built for Lodoss are restored from the local binary cache.

## Tests

```powershell
python -m pytest
python -m pytest --require-private-inputs   # fail instead of skip without the disc
```

## Legal boundary

Only process a disc you are entitled to use. Never commit or distribute the
disc image, `default.xbe`, extracted assets, BIOS/MCPX/EEPROM/HDD images, or
Xbox SDK libraries. The ignore rules exclude the default locations.
