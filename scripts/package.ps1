# SPDX-FileCopyrightText: 2026 WIN Infinite Desktop contributors
# SPDX-License-Identifier: GPL-3.0-or-later
#
# Collects a build into a folder that runs on its own (Qt deployed next to the programs)
# and zips it: win-infinite-desktop-<version>-<arch>.zip with install.ps1 at its root.
# Used by CI (artifact of every run) and by the release workflow (release asset).
#
# Usage: pwsh scripts/package.ps1 [-Build build] [-Output dist] [-Arch x64]
# windeployqt must be on PATH (Qt's bin folder).

param(
    [string]$Build = "build",
    [string]$Output = "dist",
    [string]$Arch = "x64"
)

$ErrorActionPreference = "Stop"
$root = Split-Path $PSScriptRoot

$version = (Select-String -Path (Join-Path $root "CMakeLists.txt") -Pattern '^project\(win-infinite-desktop VERSION ([0-9.]+)').Matches[0].Groups[1].Value
if (-not $version) { throw "no project version in CMakeLists.txt" }

# Programs of the package; the apps are optional (a build without Qt Quick has none).
$required = @("win-infinite-desktop.exe", "wid-ctl.exe", "wid-probe.exe")
$optional = @("wid-notes.exe", "wid-settings.exe")

if (Test-Path $Output) { Remove-Item -Recurse -Force $Output }
New-Item -ItemType Directory -Force $Output | Out-Null

function FindProgram([string]$name) {
    Get-ChildItem -Path $Build -Recurse -File -Filter $name -ErrorAction SilentlyContinue |
        Where-Object { $_.FullName -notmatch '\\(CMakeFiles|tests?)\\' } | Select-Object -First 1
}

$deployed = @()
foreach ($name in $required + $optional) {
    $program = FindProgram $name
    if (-not $program) {
        if ($required -contains $name) { throw "$name is missing in $Build" }
        Write-Host "not built: $name"
        continue
    }
    Copy-Item $program.FullName $Output
    $deployed += (Join-Path $Output $name)
}

$qtPrograms = $deployed | Where-Object { (Split-Path $_ -Leaf) -ne "wid-probe.exe" }
$quick = $deployed | Where-Object { $optional -contains (Split-Path $_ -Leaf) }
$arguments = @("--release", "--no-translations", "--no-system-d3d-compiler", "--no-opengl-sw", "--no-compiler-runtime")
if ($quick) {
    # The QML apps: deploy the QML modules their files import.
    $arguments += @("--qmldir", (Join-Path $root "src"))
} else {
    $arguments += "--no-quick-import"
}
& windeployqt @arguments @qtPrograms
if ($LASTEXITCODE -ne 0) { throw "windeployqt failed" }

# The MSVC runtime: the CI build links it dynamically; ship the redistributable DLLs.
if ($env:VCToolsRedistDir) {
    $crt = Get-ChildItem -Path (Join-Path $env:VCToolsRedistDir $Arch) -Directory -Filter "Microsoft.VC*.CRT" -ErrorAction SilentlyContinue |
        Select-Object -First 1
    if ($crt) { Copy-Item (Join-Path $crt.FullName "*.dll") $Output }
}

Copy-Item (Join-Path $root "scripts/install.ps1"), (Join-Path $root "LICENSE"), (Join-Path $root "README.md") $Output

$zip = "win-infinite-desktop-$version-$Arch.zip"
if (Test-Path $zip) { Remove-Item $zip }
Compress-Archive -Path (Join-Path $Output "*") -DestinationPath $zip
Write-Host "package: $zip ($([int]((Get-Item $zip).Length / 1MB)) MB)"
if ($env:GITHUB_OUTPUT) {
    Add-Content -Path $env:GITHUB_OUTPUT -Value "zip=$zip"
    Add-Content -Path $env:GITHUB_OUTPUT -Value "version=$version"
}
