# SPDX-FileCopyrightText: 2026 WIN Infinite Desktop contributors
# SPDX-License-Identifier: GPL-3.0-or-later
#
# Records the images of the documentation in Windows Sandbox: packages a build, starts a
# sandbox with it, plays scenes.ps1 there and turns the animation frames into WebP files.
# The desktop of the host is not touched; the sandbox window shows what happens.
#
# Usage: powershell -File tools/wid-media/record.ps1 [-Build build] [-Output docs\images]
#                   [-Scenes all] [-Theme dark|light] [-Keep]
# Needs: Windows Sandbox (wsb.exe), windeployqt on PATH (Qt's bin folder) and ffmpeg on PATH
# for the animations (without it the PNG frames stay in the work folder).

param(
    [string]$Build = "build",
    [string]$Output = "",
    [string[]]$Scenes = @("all"),
    [ValidateSet("dark", "light")][string]$Theme = "dark",
    # Keep the sandbox and the work folder for a look afterwards.
    [switch]$Keep
)

# Native programs report on stderr (windeployqt warns there); failures are checked by exit code.
$ErrorActionPreference = "Continue"
$root = Split-Path (Split-Path $PSScriptRoot)
if (-not $Output) { $Output = Join-Path $root "docs\images" }
$Build = (Resolve-Path $Build).Path
New-Item -ItemType Directory -Force $Output | Out-Null
$work = Join-Path ([IO.Path]::GetTempPath()) ("wid-media-" + [Guid]::NewGuid().ToString("N"))
New-Item -ItemType Directory -Force "$work\out" | Out-Null

# The MSVC runtime goes into the package (a fresh sandbox has none); outside of a developer
# prompt its folder comes from the Visual Studio installation.
if (-not $env:VCToolsRedistDir) {
    $vswhere = Join-Path ${env:ProgramFiles(x86)} "Microsoft Visual Studio\Installer\vswhere.exe"
    $vs = if (Test-Path $vswhere) { & $vswhere -latest -products * -property installationPath } else { $null }
    $versionFile = if ($vs) { Join-Path $vs "VC\Auxiliary\Build\Microsoft.VCRedistVersion.default.txt" } else { "" }
    if ($versionFile -and (Test-Path $versionFile)) {
        $env:VCToolsRedistDir = Join-Path $vs ("VC\Redist\MSVC\" + (Get-Content $versionFile -First 1).Trim() + "\")
    } else {
        Write-Host "no Visual Studio found: the package has no MSVC runtime, the sandbox cannot start the programs"
    }
}

# The programs as they are shipped, plus wid-media and the scene script.
# In a process of its own: Windows PowerShell 5.1 would stop at windeployqt's warnings on stderr.
powershell.exe -NoProfile -ExecutionPolicy Bypass -File (Join-Path $root "scripts\package.ps1") -Build $Build -Output "$work\app"
if ($LASTEXITCODE -ne 0) { throw "packaging failed" }
$media = Get-ChildItem -Path $Build -Recurse -Filter "wid-media.exe" | Select-Object -First 1
if (-not $media) { throw "wid-media.exe is missing in $Build" }
Copy-Item $media.FullName "$work\app"
Copy-Item (Join-Path $PSScriptRoot "scenes.ps1") $work

$config = "<Configuration><vGPU>Enable</vGPU><Networking>Disable</Networking><MemoryInMB>4096</MemoryInMB>" +
    "<MappedFolders><MappedFolder><HostFolder>$work</HostFolder><SandboxFolder>C:\wid</SandboxFolder><ReadOnly>false</ReadOnly></MappedFolder></MappedFolders></Configuration>"
$id = (wsb start --config $config --raw | Out-String | ConvertFrom-Json).Id
if (-not $id) { throw "Windows Sandbox did not start" }
Write-Host "sandbox $id"
try {
    # Commands need a signed-in session, which the sandbox window brings.
    Start-Process -FilePath "wsb" -ArgumentList "connect", "--id", $id -WindowStyle Minimized
    $ready = $false
    for ($i = 0; $i -lt 60 -and -not $ready; $i++) {
        Start-Sleep -Seconds 2
        wsb exec --id $id -c "cmd.exe /c exit 0" -r ExistingLogin --raw 2>$null | Out-Null
        $ready = $LASTEXITCODE -eq 0
    }
    if (-not $ready) { throw "no session in the sandbox" }
    Start-Sleep -Seconds 5
    # wsb passes the command on without quotes of its own: a script file avoids quoting.
    Set-Content -Path "$work\run.ps1" -Encoding utf8 -Value @"
try {
    & C:\wid\scenes.ps1 -App C:\wid\app -Out C:\wid\out -Scenes $($Scenes -join ',') -Theme $Theme *> C:\wid\out\transcript.txt
} catch {
    Add-Content -Path C:\wid\out\transcript.txt -Value "ERROR: `$_"
}
Set-Content -Path C:\wid\out\finished.txt -Value done
"@
    wsb exec --id $id -c "powershell.exe -NoProfile -ExecutionPolicy Bypass -File C:\wid\run.ps1" -r ExistingLogin --raw | Out-Null
    for ($i = 0; $i -lt 900 -and -not (Test-Path "$work\out\finished.txt"); $i++) { Start-Sleep -Seconds 1 }
    Get-Content "$work\out\scenes.log" -ErrorAction SilentlyContinue
    Get-Content "$work\out\transcript.txt" -ErrorAction SilentlyContinue | Where-Object { $_ -match "ERROR|Exception" }
} finally {
    if (-not $Keep) { wsb stop --id $id | Out-Null }
}

# Stills as they are (whole screens with the wallpaper photo as WebP), animations as WebP
# (8 frames per second, 1024 pixels wide).
$ffmpeg = Get-Command ffmpeg -ErrorAction SilentlyContinue
foreach ($still in Get-ChildItem "$work\out" -Filter "*.png") {
    if ($ffmpeg -and @("overview", "map") -contains $still.BaseName) {
        & $ffmpeg.Source -loglevel error -y -i $still.FullName -vf "scale=1600:-1:flags=lanczos" -c:v libwebp -quality 85 (Join-Path $Output "$($still.BaseName).webp")
    } else {
        Copy-Item $still.FullName $Output -Force
    }
}
foreach ($frames in Get-ChildItem "$work\out" -Directory -Filter "frames-*") {
    $name = @{ "frames-zoom" = "zoom-overview"; "frames-pan" = "canvas-pan" }[$frames.Name]
    if (-not $name) { $name = $frames.Name -replace '^frames-', '' }
    if ($ffmpeg) {
        & $ffmpeg.Source -loglevel error -y -framerate 8 -i (Join-Path $frames.FullName "%03d.png") -vf "scale=1024:-1:flags=lanczos" `
            -loop 0 -c:v libwebp -quality 60 -compression_level 6 (Join-Path $Output "$name.webp")
    } else {
        Write-Host "ffmpeg is not on PATH: the frames of $name stay in $($frames.FullName)"
        $Keep = $true
    }
}
if ($Keep) { Write-Host "work folder: $work" } else { Remove-Item -Recurse -Force $work }
Write-Host "images: $Output"
