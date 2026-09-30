"""Package a built Windows release without including user game content."""

from __future__ import annotations

import hashlib
import json
from pathlib import Path
import subprocess
import zipfile


ROOT = Path(__file__).resolve().parents[1]
VCPKG = ROOT / ".tools/vcpkg"
INSTALLED = ROOT / "build/port/vcpkg_installed/x64-windows"
BIN = ROOT / "build/port/port/Release"
LIBRARIES = {
    "cgltf": "jkuhlmann-cgltf-v1.15.tar.gz",
    "ffmpeg": "ffmpeg-ffmpeg-n9.0.tar.gz",
    "sdl3": "libsdl-org-SDL-release-3.4.14.tar.gz",
    "sdl3-image": "libsdl-org-SDL_image-release-3.4.4.tar.gz",
    "libpng": "pnggroup-libpng-v1.6.58.tar.gz",
    "zlib": "madler-zlib-v1.3.2.tar.gz",
}
DLLS = (
    "SDL3.dll", "SDL3_image.dll", "avcodec-63.dll", "avformat-63.dll",
    "avutil-61.dll", "swresample-7.dll", "swscale-10.dll", "libpng16.dll", "z.dll",
)


def git(*args: str) -> str:
    return subprocess.check_output(
        ["git", "-C", str(ROOT), *args], text=True, timeout=30
    ).strip()


def add(archive: zipfile.ZipFile, source: Path, name: str) -> None:
    if not source.is_file() or source.is_symlink():
        raise ValueError(f"Missing or linked package input: {source}")
    archive.write(source, name)


def main() -> None:
    manifest = json.loads((ROOT / "vcpkg.json").read_text())
    version = manifest["version-string"]
    baseline = manifest["builtin-baseline"]
    if git("-C", str(VCPKG), "rev-parse", "HEAD") != baseline:
        raise ValueError("vcpkg checkout does not match the manifest baseline")
    stem = f"PhantomCrash-{version}-windows-x64"
    output = ROOT / "build/releases"
    output.mkdir(parents=True, exist_ok=True)
    binary = output / f"{stem}.zip"
    sources = output / f"PhantomCrash-{version}-third-party-sources.zip"
    if binary.exists() or sources.exists():
        raise FileExistsError("Release archives already exist; choose a new version/output")

    files = git("ls-files", "--cached", "--others", "--exclude-standard",
                "data", "src/phantomcrash_research").splitlines()
    files = sorted(set(p for p in files if p.startswith("data/") or p.endswith(".py")))
    metadata = {
        "version": version,
        "commit": git("rev-parse", "HEAD"),
        "working_tree_changes": bool(git("status", "--porcelain")),
        "vcpkg_baseline": baseline,
        "configuration": "Release",
        "architecture": "x64",
    }
    with zipfile.ZipFile(binary, "x", zipfile.ZIP_DEFLATED, compresslevel=9) as z:
        add(z, BIN / "phantomcrash_port.exe", f"{stem}/phantomcrash_port.exe")
        for dll in DLLS:
            add(z, BIN / dll, f"{stem}/{dll}")
        for file in files + ["LICENSE", "tools/phantomcrash.py", "scripts/extract-game.ps1",
                             "docs/REPLAY.md", "docs/MATCH_RESULT.md",
                             "docs/ENTRY_SHIELD_RENDERING.md"]:
            add(z, ROOT / file, f"{stem}/{file}")
        add(z, ROOT / "docs/WINDOWS_RELEASE.md", f"{stem}/README.md")
        for library in LIBRARIES:
            add(z, INSTALLED / "share" / library / "copyright",
                f"{stem}/licenses/{library}.txt")
        z.writestr(f"{stem}/build-info.json", json.dumps(metadata, indent=2) + "\n")
        z.writestr(f"{stem}/Start Game.bat", (
            '@echo off\r\npushd "%~dp0"\r\n'
            'phantomcrash_port.exe --root "%~dp0." %*\r\n'
            'set "game_exit=%ERRORLEVEL%"\r\n'
            'if not "%game_exit%"=="0" pause\r\npopd\r\nexit /b %game_exit%\r\n'
        ))

    with zipfile.ZipFile(sources, "x", zipfile.ZIP_DEFLATED, compresslevel=9) as z:
        for library, filename in LIBRARIES.items():
            add(z, VCPKG / "downloads" / filename, f"upstream/{filename}")
            for path in sorted((VCPKG / "ports" / library).rglob("*")):
                if path.is_file():
                    add(z, path, "vcpkg/" + path.relative_to(VCPKG).as_posix())
        # FFmpeg's helper is part of the pinned build recipe too.
        for path in sorted((VCPKG / "ports/ffmpeg-bin2c").rglob("*")):
            if path.is_file():
                add(z, path, "vcpkg/" + path.relative_to(VCPKG).as_posix())
        for file in ("config.h", "config_components.h", "ffbuild/config.mak"):
            add(z, VCPKG / "buildtrees/ffmpeg/x64-windows-rel" / file,
                f"ffmpeg-build/{file}")
        for file in ("vcpkg.json", "scripts/build-port.ps1", "docs/WINDOWS_RELEASE.md"):
            add(z, ROOT / file, file)
        z.writestr("README.txt", (
            f"Library sources for Phantom Crash {version} Windows x64.\n"
            "Upstream archives are unmodified. Apply the patches in each port's\n"
            "portfile.cmake in order, or use the port build script in the source tag.\n"
            f"vcpkg baseline: {baseline}\n"
            "https://github.com/microsoft/vcpkg/tree/" + baseline + "\n"
            "FFmpeg configuration is included under ffmpeg-build.\n"
            "FFmpeg is built with the MSVC dynamic CRT, without GPL/nonfree features.\n"
        ))

    checksums = []
    for path in (binary, sources):
        with path.open("rb") as stream:
            digest = hashlib.file_digest(stream, "sha256").hexdigest()
        checksums.append(f"{digest}  {path.name}\n")
        print(f"{path.name}: {path.stat().st_size:,} bytes")
    (output / "SHA256SUMS.txt").write_text("".join(checksums), encoding="utf-8")


if __name__ == "__main__":
    main()
