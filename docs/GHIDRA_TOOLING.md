# Ghidra and reverse-engineering toolchain

Game data, external source checkouts, Ghidra projects and analysis logs stay
ignored; only reproducible setup, version pins and conclusions belong in Git.

## Baseline

| Component | Location / version | Role |
|---|---|---|
| Ghidra | `C:\Tools\ghidra_12.1.2_PUBLIC_20260605\ghidra_12.1.2_PUBLIC` / 12.1.2 | Static analysis and decompilation of the x86 guest code |
| Java | `C:\Program Files\Microsoft\jdk-21.0.11.10-hotspot` / 21 | Explicit Ghidra runtime; the default `java` on `PATH` is Java 8 |
| ghidra-xbe | `.tools\ghidra\ghidra-xbe`, pinned in `tools\ghidra\external-tools.json` | XBE loader: sections, kernel thunks, SDK naming |
| XbSymbolDatabase | `.tools\ghidra\XbSymbolDatabase`, pinned | SDK signature matching; CLI built from source and bundled into ghidra-xbe |
| GhidraMCP | installed Ghidra extension | Optional interactive inspection |
| radare2 | `C:\Tools\radare2-6.1.8-w64\bin\radare2.exe` | Independent cross-check |
| xemu_decomp | `C:\Projects\Xemu\xemu_decomp` | Runtime evidence (reference emulator) |

## Setup

```powershell
pwsh -NoProfile -File .\scripts\setup-ghidra-tools.ps1
pwsh -NoProfile -File .\scripts\build-ghidra-xbe.ps1 -Install
pwsh -NoProfile -File .\scripts\analyze-ghidra.ps1
```

`setup-ghidra-tools.ps1` clones both repositories at their pinned commits and
refuses modified or wrong-origin checkouts (`-CheckOnly` only verifies).

`build-ghidra-xbe.ps1` exists because the newest upstream ghidra-xbe release
(`build-202602250354`) targets Ghidra 12.0.3, and Ghidra extensions only load
into the exact version they were built for. The script:

1. builds `XbSymbolDatabaseCLI.exe` from the pinned source with CMake and MSVC;
2. copies the loader source into `.tools\ghidra\build\ghidra-xbe`, so the pinned
   checkout stays clean, and bundles the CLI and its license;
3. generates an empty XTLID table instead of downloading `xtlid.xml`. XTLID
   naming only applies to XBEs with an `.XTLID` section, which this game lacks;
4. runs Gradle (a cached distribution under `%USERPROFILE%\.gradle`, else
   Ghidra's bundled `support\gradle\gradlew.bat`) with Java 21;
5. with `-Install`, unpacks the package to
   `%APPDATA%\ghidra\ghidra_12.1.2_PUBLIC\Extensions\ghidra-xbe`. Delete that
   folder to uninstall.

`analyze-ghidra.ps1` imports `artifacts\europe\files\default.xbe` with the
`XbeLoader`, runs full auto-analysis including the XbSymbolDatabase analyzer,
then runs `tools\ghidra\ExportXbeSummary.java`. Results go to ignored
`research-output\ghidra`: the project, console and script logs, and
`phantomcrash-europe-summary\{summary.json,functions.csv}`. It refuses to
overwrite an existing project name. `analyzeHeadless` can exit 0 after a
script failure, so the wrapper also checks the log and the summary file.

`tools\ghidra\DecompileFunctions.java` decompiles chosen functions from the
analysed project into one `.c` file each. Append `+` to an address to include
its direct callees. The script creates a function where none exists, which
covers task callbacks reached only through data pointers; with `-readOnly`
those functions are not saved. Bound the run with a timeout:

```powershell
analyzeHeadless.bat research-output\ghidra phantomcrash-europe -process default.xbe `
  -noanalysis -readOnly -scriptPath tools\ghidra `
  -postScript DecompileFunctions.java <output-dir> 2c460+ a2268
```

## Naming provenance

SDK names from XbSymbolDatabase are library identifications, not recovered
game-source symbols. Game-owned functions remain unnamed until their behaviour
is understood; decompiler output is a model of machine code, not the original
source.
