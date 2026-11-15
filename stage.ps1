param([Parameter(Mandatory=$true)][string]$BaseMod)
$ErrorActionPreference = 'Stop'
$base = (Resolve-Path -LiteralPath $BaseMod).Path
$source = Join-Path $PSScriptRoot 'package\BowMod'
$dll = Join-Path $PSScriptRoot 'development\BowSkills.dll'
$assets = Get-Content -LiteralPath (Join-Path $PSScriptRoot 'work\runtime-assets.txt')
$destination = Join-Path $PSScriptRoot 'dist\BowMod'
if (Test-Path -LiteralPath $destination) { throw 'Use an empty output location. Move the previous dist/BowMod before staging again.' }
if (!(Test-Path -LiteralPath $dll -PathType Leaf)) { throw 'Run build.ps1 first.' }
if (!(Test-Path -LiteralPath (Join-Path $base 'BowMod.mod') -PathType Leaf)) { throw 'BaseMod must point to a copy of the installed BowMod.' }
$missing = @($assets | Where-Object { !(Test-Path -LiteralPath (Join-Path $base $_) -PathType Leaf) })
if ($missing.Count) { throw "BaseMod is missing required assets: $($missing -join ', ')" }
New-Item -ItemType Directory -Path $destination | Out-Null
foreach ($asset in $assets) {
    $target = Join-Path $destination $asset
    New-Item -ItemType Directory -Force -Path (Split-Path $target) | Out-Null
    Copy-Item -LiteralPath (Join-Path $base $asset) -Destination $target
}
Copy-Item -Path "$source\*" -Destination $destination -Recurse
Copy-Item -LiteralPath $dll -Destination $destination
Write-Output $destination
