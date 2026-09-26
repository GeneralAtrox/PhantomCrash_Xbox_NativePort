#requires -Version 7.4

# Build the ghidra-xbe loader for the local Ghidra version from the pinned
# checkouts, bundling an XbSymbolDatabaseCLI built from pinned source.
# With -Install, the extension is unpacked into the user's Ghidra settings.

[CmdletBinding()]
param(
    [switch]$Install,
    [string]$GradleCommand = ''
)

$ErrorActionPreference = 'Stop'
$repositoryRoot = (Resolve-Path (Join-Path $PSScriptRoot '..')).Path
$manifest = Get-Content -Raw -LiteralPath (Join-Path $repositoryRoot 'tools\ghidra\external-tools.json') |
    ConvertFrom-Json
$toolsRoot = Join-Path $repositoryRoot '.tools\ghidra'
$buildRoot = Join-Path $toolsRoot 'build'
$ghidraRoot = [string]$manifest.ghidra_root
$javaHome = [string]$manifest.java_home

function Invoke-Checked {
    param([string]$Command, [string[]]$Arguments)
    & $Command @Arguments
    if ($LASTEXITCODE -ne 0) {
        throw "$Command $($Arguments -join ' ') failed with exit code $LASTEXITCODE"
    }
}

& (Join-Path $PSScriptRoot 'setup-ghidra-tools.ps1') -CheckOnly | Out-Null

$version = ((Get-Content -LiteralPath (Join-Path $ghidraRoot 'Ghidra\application.properties') |
    Where-Object { $_ -match '^application\.version=' }) -split '=', 2)[1].Trim()
if ($version -ne $manifest.tested_ghidra_version) {
    throw "Ghidra $version is untested; expected $($manifest.tested_ghidra_version)."
}

# 1. XbSymbolDatabaseCLI from the pinned source.
$xbsdbBuild = Join-Path $buildRoot 'XbSymbolDatabase'
Invoke-Checked cmake @('-S', (Join-Path $toolsRoot 'XbSymbolDatabase'), '-B', $xbsdbBuild,
    '-G', 'Visual Studio 17 2022', '-A', 'x64', '-DXBSDB_BUILD_UNITTEST=OFF')
Invoke-Checked cmake @('--build', $xbsdbBuild, '--config', 'Release', '--target', 'XbSymbolDatabaseCLI')
$cli = Join-Path $xbsdbBuild 'projects\cli\Release\XbSymbolDatabaseCLI.exe'

# 2. Stage a copy of the loader source so the pinned checkout stays clean.
$staging = Join-Path $buildRoot 'ghidra-xbe'
if (Test-Path -LiteralPath $staging) {
    Remove-Item -Recurse -Force -LiteralPath $staging
}
Copy-Item -Recurse -LiteralPath (Join-Path $toolsRoot 'ghidra-xbe') -Destination $staging
Remove-Item -Recurse -Force -LiteralPath (Join-Path $staging '.git')
Copy-Item -LiteralPath $cli -Destination (Join-Path $staging 'os\win_x86_64\XbSymbolDatabaseCLI.exe')
Copy-Item -LiteralPath (Join-Path $toolsRoot 'XbSymbolDatabase\LICENSE') `
    -Destination (Join-Path $staging 'os\win_x86_64\XbSymbolDatabaseCLI.LICENSE')

# 3. The XTLID table is generated from a separately released xtlid.xml. It only
#    applies to XBEs with an .XTLID section, which Phantom Crash does not have,
#    so an empty table is generated instead of downloading it.
@'
package xbeloader;

import java.util.HashMap;
import java.util.Map;

public class XbeXtlidDb {
    public static final Map<Long, String[]> xtlids = new HashMap<>();
}
'@ | Set-Content -Encoding utf8 -LiteralPath (Join-Path $staging 'src\main\java\XbeLoader\XbeXtlidDb.java')

# 4. Build with a Gradle that satisfies Ghidra's minimum (prefer a cached one).
if (-not $GradleCommand) {
    $cached = Get-ChildItem -Path (Join-Path $HOME '.gradle\wrapper\dists') -Recurse -Filter 'gradle.bat' `
        -ErrorAction SilentlyContinue | Sort-Object FullName -Descending | Select-Object -First 1
    $GradleCommand = if ($cached) { $cached.FullName } else { Join-Path $ghidraRoot 'support\gradle\gradlew.bat' }
}
$oldJavaHome = $env:JAVA_HOME
$oldGhidra = $env:GHIDRA_INSTALL_DIR
try {
    $env:JAVA_HOME = $javaHome
    $env:GHIDRA_INSTALL_DIR = $ghidraRoot
    Push-Location $staging
    Invoke-Checked $GradleCommand @('--no-daemon', '-q', 'buildExtension')
}
finally {
    Pop-Location
    $env:JAVA_HOME = $oldJavaHome
    $env:GHIDRA_INSTALL_DIR = $oldGhidra
}
$package = Get-ChildItem -LiteralPath (Join-Path $staging 'dist') -Filter '*.zip' | Select-Object -First 1
if (-not $package) {
    throw "Gradle finished but produced no extension package in $staging\dist."
}

# 5. Optional install into the per-user Ghidra extensions directory.
$installed = $null
if ($Install) {
    $extensions = Join-Path $env:APPDATA "ghidra\ghidra_${version}_PUBLIC\Extensions"
    New-Item -ItemType Directory -Force -Path $extensions | Out-Null
    $target = Join-Path $extensions 'ghidra-xbe'
    if (Test-Path -LiteralPath $target) {
        Remove-Item -Recurse -Force -LiteralPath $target
    }
    Expand-Archive -LiteralPath $package.FullName -DestinationPath $extensions
    $installed = $target
}

[pscustomobject]@{
    Status = 'built'
    GhidraVersion = $version
    Package = $package.FullName
    InstalledTo = $installed
}
