#requires -Version 7.4

[CmdletBinding()]
param(
    [string]$Disc = (Join-Path $PSScriptRoot '..\..\Games\Phantom Crash (Europe).xiso.iso'),
    [string]$Output = (Join-Path $PSScriptRoot '..\artifacts\europe')
)

$ErrorActionPreference = 'Stop'
$cli = Join-Path $PSScriptRoot '..\tools\phantomcrash.py'
python $cli extract --disc $Disc --output $Output --quiet
if ($LASTEXITCODE -ne 0) {
    throw "Extraction failed with exit code $LASTEXITCODE"
}
python $cli convert --files (Join-Path $Output 'files') --output (Join-Path $Output 'assets') --quiet
if ($LASTEXITCODE -ne 0) {
    throw "Asset conversion failed with exit code $LASTEXITCODE"
}
