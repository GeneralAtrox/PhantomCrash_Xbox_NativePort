# Windows release

This is a work-in-progress Windows x64 port. The ZIP includes
`phantomcrash_port.exe`, the game data installer `phantomcrash_extract.exe`,
their libraries and editable `data`. Extract the entire ZIP before running it.
No game assets, executable, disc image or Xbox firmware are included, apart
from the executable icon, which contains a render of game artwork (see
NOTICE.md); the game is installed from your own European Xbox disc image. Python is not
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
opens the Class D rental shortcut (`rumbling_d_night_rental` for the night
one); it uses and saves the current quest.

## Included changes

0.5.5 fixes:

- Enabled the original RUMBLING entrance music cue previously suppressed by
  inherited tutorial settings.
- Restored the MISSILE ALERT border, durability/O.C. frame outlines and the
  multiplication sign in the SHOOTDOWN counter.
- Added ordinary damage notifications to the scrolling HUD ticker and made
  online HUD notifications follow confirmed battle events.
- Added skull markers for credited defeated robot identities when they return
  during the battle.
- Made durability warning text heavier and centered within its striped panel.

0.5.4 fixes:

- Corrected left/right stick rotation of the town map rider while preserving
  the corrected up/down controls.

0.5.3 fixes:

- Optical camouflage uses the equipped modules' active and recharge timings.
  The O.C. gauge flashes during recovery rather than while discharging.
- Dodge animations lean towards the dodge direction.
- Occupied save slots show the saved robot's equipped CHIP portrait.
- Pepper's training dialogue extends the blue upper-left edge correctly.
- The town map keeps its upper-right border black and corrects vertical rider
  rotation on the board.


0.5.2 adds:

- **Automatic LAN discovery in COOP.** Choose Host network game on one PC, then
  select its automatically listed name on another. Discovery searches each
  active IPv4 network, including alongside VPN adapters. Local split-screen
  remains available, with separate network and local saves.
- **S.L.A.I. content toggle.** Settings > S.L.A.I. content is off by default.
  First enable requires your European S.L.A.I. PS2 ISO (SLES-52940). A bundled
  installer converts the assets into `artifacts/slai/assets`; no Python install
  is required. The ISO is only needed for installation. Later toggles reuse the
  installed assets and take effect after restarting the game.
- **Weapon fixes.** Homing locks drop when a target leaves the chip's lock box;
  non-homing shoulder weapons follow the reticle instead of an off-screen lock.
  Firing displays muzzle flashes from the existing converted models.
- **Audio cues.** The pause menu plays its opening cue while gameplay audio is
  paused, and acquiring a new homing target plays the acquisition cue.
- **Garage and shop updates.** Twelve hangar slots, S.L.A.I. starter robots in
  the Fill Hangars cheat when enabled, and label/layout fixes.

S.L.A.I. and the base game have separate campaign/profile files. A new S.L.A.I.
campaign starts fresh; switching off returns to the base-game campaign. Original
settings and saves are retained. If an older, manually assembled S.L.A.I. mod is
found in `mods`, that legacy overlay is retained on disk but no longer loaded
automatically; new base and S.L.A.I. profiles are kept separately from its saves.
Both peers must enable the same content when playing online.

0.5.1 fixes:

- Selected main-menu and Quest-submenu buttons pulse yellow with dark text,
  using the options menu's selection timing.
- New Game plays the missing second cinematic, `opening02.sfd`, before the
  opening conversation. Each cinematic can be skipped separately.
- The CURRENT MUSIC meter animates during dialogue scenes.

0.5.0 adds:

- **Experimental online co-op.** Up to three players share deterministic combat
  with rollback prediction. Menus send player presence, and active matches support
  late joining and recovery from a network outage while the games remain open.
  Rollback does not replay confirmed sounds/effects or award results twice.
- **Matching-content checks.** Peers must use the same executable, gameplay data,
  extracted resources and mods, including external model buffers.
- **S.L.A.I. import support.** Native support for the optional parts, stages,
  animation, audio and effects overlay built from your own S.L.A.I. disc. Imported
  content is not included in this ZIP; see `docs/SLAI_INTEGRATION.md`.
- AI/navigation, robot motion and rendering updates from the current source,
  bounded shop labels, and an executable icon.

Online play is a prerelease feature. Local desktop gameplay passed 35–65 ms
delay each way, 3% packet loss and a thirteen-second blackout; different-machine
and Internet testing remains outstanding. Joining pauses the match while its
history is reconstructed, and confirmed sounds/effects can be delayed by latency.
Reconnection does not restore a closed game or migrate the host.

### Starting online play

Install the same release and matching content on each machine. In version 0.5.2,
open COOP, choose **Host network game** on one PC, then select its automatically
listed name on the other PC. Allow the game through the private-network firewall:
UDP 29642 for discovery and UDP 29641 for the hosted game. Both PCs must be on the
same broadcast network. Games running in solo mode are not advertised.

For a manually addressed connection, from the extracted release directory the host runs:

```powershell
.\phantomcrash_port.exe --root . --online-host 29641
```

Other players replace `HOST_IPV4` with the host's reachable address:

```powershell
.\phantomcrash_port.exe --root . --online-join HOST_IPV4:29641
```

Allow UDP 29641 to the host; Internet play needs a reachable route or port
forwarding. Each player enters the same arena through the normal menus. Keep
the games open when testing a brief network outage. More implementation and
test details are in `docs/ONLINE_MULTIPLAYER_DETERMINISM.md`.

0.4.1 adds:

- **Smoother frames.** Removed the stutters seen in busy matches: replay
  recording compresses and saves frames on a separate thread; the robots
  that may enter a Rumbling match are loaded when it starts instead of when
  they arrive; sharp text no longer stalls the frame its first text appears
  in; and a computer robot rejoining its route no longer runs over a hundred
  line-of-sight tests in one tick. In a ten-robot match the worst frames
  fell from 27�76 ms to about 8�10 ms.

Existing users: copy your `artifacts` and `mods` folders into the new
version. The converted assets are unchanged.

0.4.0 added:

- **Dynamic lighting at night.** Ship and carrier searchlights sweep the
  arena and cast shadows; entrance lamps replace the painted light pools;
  fires, blasts, muzzle flashes, projectiles and beams light their
  surroundings, and the strongest fires cast shadows.
- **Light beams in the air.** On night stages the searchlights and lamps
  show as beams through the haze, and fires glow in the air around them
  (Settings > Visuals, "Light beams in the air"; needs Dynamic lighting).
- **Smarter opponents.** The AI plans routes over the arena with jumps and
  drops, frees itself when stuck and seeks high ground; ten-robot matches no
  longer drop frames.
- **Pause menu** with RESUME and EXIT (EXIT asks to confirm).
- **Night rental match** in Level Select.
- **Sharp text** option, drawing the menus' text with the MS Gothic font
  installed with Windows (the font is not included).
- Robots no longer stick in a jump on stepped ledges, kerbs and wreck rims.
- The entry shield rises and falls at the original's speed; talk scenes no
  longer show the HUD; menu cursor sounds match the original.
- Optic camouflage hides the robot's shadow; the arm-loss crackle plays
  once; shot marks fit the surfaces they hit and no longer cover fires.

Existing users: copy your `artifacts` and `mods` folders into the new
version. The converted assets are unchanged.

0.3.0 added:

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
build `phantomcrash_slai_audio` and the bundled importer:

```powershell
python -m venv .tools/slai-builder
.tools/slai-builder/Scripts/python.exe -m pip install -r tools/slai-installer-requirements.txt
.tools/slai-builder/Scripts/python.exe tools/build_slai_installer.py
```

Run `python tools/package_release.py` (`--output DIR` writes elsewhere). Packaging
uses `build/port`, the Visual Studio C++ Redist for the runtime DLLs the
binaries import, and the pinned `.tools/vcpkg` sources, and writes the two
ZIPs plus SHA-256 checksums to `build/releases`. Only the port, data, tooling
and library files on its allowlist are included; extracted game content is
excluded.
