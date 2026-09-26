#requires -Version 7.4

# Create a new headless Ghidra project for the extracted default.xbe using the
# ghidra-xbe loader (XbSymbolDatabase SDK naming), then export a function
# summary. Requires scripts\build-ghidra-xbe.ps1 -Install to have been run.

[CmdletBinding()]
param(
    [string]$Executable = (Join-Path $PSScriptRoot '..\artifacts\europe\files\default.xbe'),
    [string]$ProjectDirectory = (Join-Path $PSScriptRoot '..\research-output\ghidra'),
    [ValidatePattern('^[A-Za-z0-9_.-]+$')]
    [string]$ProjectName = 'phantomcrash-europe',
    [ValidateRange(1, 256)]
    [int]$MaxCpu = [Math]::Min([Environment]::ProcessorCount, 8),
    [ValidateRange(60, 7200)]
    [int]$TimeoutSeconds = 1800
)

$ErrorActionPreference = 'Stop'
$repositoryRoot = (Resolve-Path (Join-Path $PSScriptRoot '..')).Path
$manifest = Get-Content -Raw -LiteralPath (Join-Path $repositoryRoot 'tools\ghidra\external-tools.json') |
    ConvertFrom-Json
$ghidraRoot = [string]$manifest.ghidra_root
$javaHome = [string]$manifest.java_home
$analyzeHeadless = Join-Path $ghidraRoot 'support\analyzeHeadless.bat'

if (-not (Test-Path -LiteralPath $Executable -PathType Leaf)) {
    throw "Executable not found: $Executable. Run scripts\extract-game.ps1 first."
}
$extension = Join-Path $env:APPDATA "ghidra\ghidra_$($manifest.tested_ghidra_version)_PUBLIC\Extensions\ghidra-xbe"
if (-not (Test-Path -LiteralPath $extension)) {
    throw "ghidra-xbe is not installed. Run scripts\build-ghidra-xbe.ps1 -Install first."
}

New-Item -ItemType Directory -Force -Path $ProjectDirectory | Out-Null
$projectPath = (Resolve-Path -LiteralPath $ProjectDirectory).Path
if ((Test-Path -LiteralPath (Join-Path $projectPath "$ProjectName.gpr")) -or
    (Test-Path -LiteralPath (Join-Path $projectPath "$ProjectName.rep"))) {
    throw "Ghidra project already exists. Keep it and choose a new -ProjectName: $ProjectName"
}
$summaryDirectory = Join-Path $projectPath "$ProjectName-summary"
$consoleLog = Join-Path $projectPath "$ProjectName-console.log"
$scriptLog = Join-Path $projectPath "$ProjectName-script.log"

$arguments = @(
    $projectPath, $ProjectName,
    '-import', (Resolve-Path -LiteralPath $Executable).Path,
    '-loader', 'XbeLoader',
    '-analysisTimeoutPerFile', $TimeoutSeconds.ToString(),
    '-max-cpu', $MaxCpu.ToString(),
    '-scriptPath', (Join-Path $repositoryRoot 'tools\ghidra'),
    '-postScript', 'ExportXbeSummary.java', $summaryDirectory,
    '-scriptlog', $scriptLog
)

$oldJavaHome = $env:JAVA_HOME
$oldPath = $env:PATH
try {
    $env:JAVA_HOME = $javaHome
    $env:PATH = (Join-Path $javaHome 'bin') + [IO.Path]::PathSeparator + $oldPath
    $output = @(& $analyzeHeadless @arguments 2>&1)
    $exitCode = $LASTEXITCODE
}
finally {
    $env:JAVA_HOME = $oldJavaHome
    $env:PATH = $oldPath
}
$output | Set-Content -Encoding utf8 -LiteralPath $consoleLog

# analyzeHeadless can exit 0 after a script failure, so check the log text too.
$log = ($output -join [Environment]::NewLine)
if ($exitCode -ne 0 -or $log -match 'SCRIPT ERROR|Import failed|Analysis failed' -or
    $log -notmatch 'REPORT: Import succeeded' -or
    -not (Test-Path -LiteralPath (Join-Path $summaryDirectory 'summary.json'))) {
    throw "Ghidra analysis failed; inspect $consoleLog and $scriptLog."
}

[pscustomobject]@{
    Status = 'complete'
    Project = Join-Path $projectPath "$ProjectName.gpr"
    Summary = Join-Path $summaryDirectory 'summary.json'
    Functions = Join-Path $summaryDirectory 'functions.csv'
}
