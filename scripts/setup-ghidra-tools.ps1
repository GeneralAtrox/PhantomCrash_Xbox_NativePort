#requires -Version 7.4

# Clone or verify every enabled external Ghidra tool at its pinned commit under
# the ignored .tools\ghidra directory. Nothing is vendored into Git.

[CmdletBinding()]
param(
    [string]$ToolsRoot = (Join-Path $PSScriptRoot '..\.tools\ghidra'),
    [switch]$CheckOnly
)

$ErrorActionPreference = 'Stop'
$repositoryRoot = (Resolve-Path (Join-Path $PSScriptRoot '..')).Path
$manifest = Get-Content -Raw -LiteralPath (Join-Path $repositoryRoot 'tools\ghidra\external-tools.json') |
    ConvertFrom-Json

function Invoke-Git {
    param([string[]]$Arguments)
    $output = @(& git @Arguments 2>&1)
    if ($LASTEXITCODE -ne 0) {
        throw "git $($Arguments -join ' ') failed:`n$($output -join [Environment]::NewLine)"
    }
    return $output
}

foreach ($tool in @($manifest.tools | Where-Object status -eq 'enabled_external')) {
    $destination = [IO.Path]::GetFullPath((Join-Path $ToolsRoot $tool.name))
    if (-not (Test-Path -LiteralPath (Join-Path $destination '.git'))) {
        if ($CheckOnly) {
            throw "Missing pinned checkout: $destination. Run without -CheckOnly."
        }
        New-Item -ItemType Directory -Force -Path (Split-Path -Parent $destination) | Out-Null
        Invoke-Git @('clone', '--quiet', '--filter=blob:none', $tool.repository, $destination) | Out-Null
    }

    $origin = (Invoke-Git @('-C', $destination, 'remote', 'get-url', 'origin') | Select-Object -Last 1).Trim()
    if ($origin.TrimEnd('/') -ne ([string]$tool.repository).TrimEnd('/')) {
        throw "$($tool.name) origin is '$origin'; expected '$($tool.repository)'."
    }
    $dirty = ((Invoke-Git @('-C', $destination, 'status', '--porcelain')) -join '').Trim()
    if ($dirty) {
        throw "Refusing to use a modified checkout: $destination"
    }
    $head = (Invoke-Git @('-C', $destination, 'rev-parse', 'HEAD') | Select-Object -Last 1).Trim()
    if ($head -ne $tool.commit) {
        if ($CheckOnly) {
            throw "$($tool.name) is at $head; expected $($tool.commit)."
        }
        Invoke-Git @('-C', $destination, 'fetch', '--quiet', 'origin', $tool.commit) | Out-Null
        Invoke-Git @('-C', $destination, 'checkout', '--quiet', '--detach', $tool.commit) | Out-Null
    }
    Invoke-Git @('-C', $destination, 'submodule', 'update', '--init', '--recursive', '--quiet') | Out-Null

    [pscustomobject]@{ Name = $tool.name; Status = 'ready'; Commit = $tool.commit; Path = $destination }
}
