# Code organization

New and refactored handwritten source/header files have a 400-physical-line
limit. Format C++ using the repository `.clang-format` before counting.
Generated tables and map/content data are excluded. Split responsibilities
and ownership; do not compress statements or split a function across included
fragments.

The formatter inserts braces for control flow, keeps nonempty functions and
branches on multiple lines, and uses two-space indentation with a 100-column
target. Apply it before checking the 400-line limit:

```powershell
& 'C:\Program Files\Microsoft Visual Studio\2022\Community\VC\Tools\Llvm\x64\bin\clang-format.exe' -i port\src\*.cpp port\include\phantomcrash\*.hpp
```

## Layout

| Path | Contents |
|---|---|
| `src/phantomcrash_research/` | Python research package: XDVDFS reader, XBE parser, extraction, CLI |
| `src/phantomcrash_research/{xbox_texture,texture_pack,png,audio,game_tables,convert}.py` | Asset conversion: Xbox textures and packs, PNG, Xbox ADPCM and SE BIN banks, XBE tables |
| `data/` | Editable port data (intro, title, menu, music, effects); paths resolve through `mods/` first |
| `tools/phantomcrash.py` | Source-checkout CLI entry point |
| `tools/ghidra/` | Pinned external-tool manifest and Ghidra Java scripts |
| `scripts/` | PowerShell entry points (extract, build, Ghidra setup/analysis) |
| `port/include/phantomcrash/`, `port/src/` | C++20 modern port |
| `port/src/{json,json_access,data_roots,input,frame_rate,settings,intro_sequence,sprite_layer,menu_cursor,front_end,front_end_script,glyph_font,message_box,menu_screen,option_menu,option_script,sound_config,button_config,screen_adjust}.cpp` | Core library (no SDL): strict JSON, data roots and mod overlay, Xbox button model, frame pacer, saved settings and game options, intro step machine, sprite layers and draw-flag animations, cursor repeat, title/menu state machine, system font and tickers, message box, shared menu-screen parts (script basics, fades, help clocks), option screen and SOUND CONFIG / BUTTON CONFIG / SCREEN ADJUST state machines |
| `port/src/movie_decoder.cpp`, `port/include/phantomcrash/movie_clock.hpp` | Media library: FFmpeg movie decoding (Sofdec `.sfd` and anything FFmpeg reads) and the audio-clock timing helpers |
| `port/src/movie_{player,audio_output}.cpp` | SDL movie playback: YUV textures, audio stream, audio-clocked frame timing |
| `port/src/desktop_*.cpp` | SDL3 desktop host: window and D3D11 renderer, input mapping, Settings menu and frame-rate dialog (from Lodoss), frame counter, texture cache, glyph drawing, layered draw list, audio mixer (music, effects, volumes), screen runner (with the screen offset), intro / front-end / option / option sub-screen / placeholder screens, main loop |
| `port/tests/` | CTest: core unit tests (self-registering harness), a movie decode test (private input, skips without the disc) and a hidden smoke run of the port |
| `tests/` | pytest suite; private-disc tests use the `private_inputs` marker |
| `docs/` | Format notes, research conclusions, tooling |
