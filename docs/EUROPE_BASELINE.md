# European baseline

Identity and first observations for the user-supplied European disc. Values
come from `python tools\phantomcrash.py extract` (`artifacts\europe\metadata`)
and the first headless Ghidra analysis.

## Disc image

| Field | Value |
|---|---|
| File | `C:\Projects\Xemu\Games\Phantom Crash (Europe).xiso.iso` |
| Size | 2,724,462,592 bytes |
| SHA-256 | `85a8fc3bf2cc7dddbe285574b386ba7639dd1a73b4cd0d6cf9abf323f869664f` |
| Layout | Trimmed xiso; XDVDFS partition at offset 0 |
| Files | 309 files, 2,723,440,970 bytes |

## default.xbe

| Field | Value |
|---|---|
| SHA-256 | `bdbe2c3eb2361395babfe6095d1a2da72f6581dccb34812b8d45e084c6c903c6` |
| Title | `phantomcrash`, title ID `0x504C0001` (`PL-001`), region flags `0x4`, version `0x4` |
| Built | 2002-10-16 21:51 UTC, retail |
| Base / entry | `0x00010000` / `0x000d8799` |
| Kernel thunk table | `0x0017a780` |
| Debug path | `K:\Release\citydiver.exe` (release build, no PDB, no RTTI) |
| XDK | build 4721: XAPILIB, D3D8, D3DX8, XGRAPHC, DSOUND, XBOXKRNL, LIBCMT |

Sections: `.text` (1.05 MiB), `D3D`, `D3DX`, `XGRPH`, `DSOUND`, `PSGSFD00`,
`PSGSFD_I`, `PSGSFD_B`, `PSGSFD_P`, `XPP`, `.rdata`, `.data` (11.4 MiB virtual,
2.2 MiB raw), `DOLBY`, `EVENTSEG` (1.5 MiB; marked executable but holds
16-bit data tables, see [the recompilation spike](RECOMP_SPIKE.md)),
`$$XTIMAGE`, `$$XSIMAGE`.

Statically linked middleware identified by version strings (shipped-data
names): CRI ADX/Sofdec for Xbox (ADXT 8.11, MWSFD 2.16, SFD 1.802, built
July 2002), IJG libjpeg, libpng and zlib.

## First Ghidra analysis

`scripts\analyze-ghidra.ps1` with the ghidra-xbe loader found 6,503
functions. XbSymbolDatabase named 294 of them (D3D8 132, DSOUND 112, XAPILIB
45, XGRAPHC 5) plus 9 kernel imports; the standalone CLI reports 349 SDK
symbols including variables.

| Section | Functions | SDK-identified | Bytes |
|---|---:|---:|---:|
| `.text` | 5,203 | 35 | 998,023 |
| `D3D` | 263 | 129 | 53,868 |
| `D3DX` | 399 | 0 | 101,649 |
| `DSOUND` | 397 | 112 | 40,311 |
| `XPP` | 193 | 13 | 25,621 |
| `XGRPH` | 32 | 5 | 7,358 |
| `PSGSFD*` | 13 | 0 | 6,520 |

`.text` still contains the C runtime, CRI middleware, libjpeg, libpng and zlib
alongside the game code. Separating those is the next triage step.

## Disc layout

| Directory | Files | Contents |
|---|---:|---|
| `/` | 1 | `default.xbe` |
| `Media/movie` | 11 | Sofdec `.sfd`: `phantagram`, `dolby`, `kudryavka`, `opening`, `opening02`, `phantomcrash`, `cloud`, `staff`, `ending`, `epilogue`, `the_end` |
| `Media/scroll` | 86 | 2D texture packs for menus and screens (`title_tex`, `logo_tex`, `system_tex`, ...) |
| `Media/binlink` | 16 | Linked model/stage packs: robots `a00_holy`, `a01_photon`, `a02_aeron`, `a03_drones`; stages `b00_shinjuku`, `b01_shibuya`, `b02_umihotaru`; effects; enemies `d00_tolomeo`, `d01_slime`, `d03_charga`; texture banks `z_robotex_*`; `zz_emblemtex`; `zzz_music` |
| `Media/model`, `Media/texture` | 17, 15 | Standalone models and textures (trophies, record cup, viewer) |
| `Media/sound` | 94 | Music as plain 16-bit 44.1 kHz stereo PCM `.wav`; effect banks `sys.bin`, `battle.bin`, `battle_player.bin`; DirectSound DSP image `dsstdfx.bin` |
| `Media/Shaders` | 53 | 28 `.xvu` vertex and 25 `.xpu` pixel shaders |
| `Media/Fonts` | 2 | `FONT.bin`, `FONT.res` |
| `Media/systemdata` | 8 | Small `.tga`/`.bmp` effect textures |
| `Media/rbdata`, `Media/enepath`, `Media/diskcheck` | 4, 1, 1 | Robot arm data, an enemy path, save image `SaveImage.xbx` |

The `scroll` texture packs and the `binlink` model packs share the same leading
bytes `02 01 00 80`, so one container parser is expected to cover both. The
executable refers to `binlink` files through the `Z:\` cache drive and to all
other files through `D:\Media\`.
