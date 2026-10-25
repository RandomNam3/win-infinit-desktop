# SPDX-FileCopyrightText: 2026 WIN Infinit Desktop contributors
# SPDX-License-Identifier: GPL-3.0-or-later
#
# Plays the scenes of the documentation images on the desktop it runs on (meant for Windows
# Sandbox, see record.ps1) and saves screenshots and animation frames as PNG.
# The service and the apps get their own, empty data folders (APPDATA/LOCALAPPDATA point to a
# temporary folder), so nothing of the user's settings or notes is read or changed.
#
# Usage: scenes.ps1 -App <folder with the programs and wid-media.exe> -Out <folder>
#                   [-Scenes overview,zoom,pan,notes,menus,map] [-Theme dark|light]

param(
    [Parameter(Mandatory)][string]$App,
    [Parameter(Mandatory)][string]$Out,
    [string[]]$Scenes = @("all"),
    [ValidateSet("dark", "light")][string]$Theme = "dark",
    # Also outside of Windows Sandbox: switches the app mode and closes File Explorer windows.
    [switch]$Force
)

$ErrorActionPreference = "Stop"
if ($env:USERNAME -ne "WDAGUtilityAccount" -and -not $Force) {
    throw "scenes.ps1 changes the desktop it runs on; run it in Windows Sandbox (record.ps1) or pass -Force"
}
$media = Join-Path $App "wid-media.exe"
$ctl = Join-Path $App "wid-ctl.exe"
New-Item -ItemType Directory -Force $Out | Out-Null
$log = Join-Path $Out "scenes.log"
Set-Content -Path $log -Value "scenes $(Get-Date -Format o)"
function Note($message) { Add-Content -Path $log -Value $message; Write-Host $message }
function Wanted($scene) { $list = @($Scenes | ForEach-Object { $_ -split "," }); $list -contains "all" -or $list -contains $scene }
function Shot($name, $rect) {
    $path = Join-Path $Out "$name.png"
    if ($rect) { & $media shot $path @rect } else { & $media shot $path }
}
function Ctl { & $ctl @args | Out-Null }

# Own data folders: the service, the notes app and the settings app read them from the environment.
$data = Join-Path ([IO.Path]::GetTempPath()) ("wid-media-" + [Guid]::NewGuid().ToString("N"))
$env:APPDATA = Join-Path $data "Roaming"
$env:LOCALAPPDATA = Join-Path $data "Local"
New-Item -ItemType Directory -Force $env:APPDATA, $env:LOCALAPPDATA | Out-Null

& $media theme $Theme
Start-Sleep -Seconds 2
# "X Y WIDTH HEIGHT work X Y WIDTH HEIGHT dpi DPI"
$screen = (& $media screen) -split ' '
$W = [int]$screen[7]; $H = [int]$screen[8]; $scale = [int]$screen[10] / 96.0
Note "work area ${W}x$H, scale $scale"
function X($f) { [int]($W * $f) }
function Y($f) { [int]($H * $f) }

# --- Programs: placed before the service starts, it takes them over where they are -------------
# Only programs that every Windows has, also Windows Sandbox (no Notepad, no Edge there).
& $media close CabinetWClass
Start-Sleep -Seconds 1
$started = @()
$started += Start-Process -FilePath (Join-Path $App "wid-settings.exe") -PassThru
Start-Sleep -Seconds 3
& $media place wid-settings.exe (X 0.04) (Y 0.07) (X 0.43) (Y 0.66)
$started += Start-Process -FilePath "msinfo32.exe" -PassThru
Start-Sleep -Seconds 3
& $media place msinfo32.exe (X 0.52) (Y 0.55) (X 0.32) (Y 0.36)
Start-Process -FilePath "explorer.exe" -ArgumentList "C:\Windows"
Start-Sleep -Seconds 3
& $media place explorer.exe (X 0.50) (Y 0.06) (X 0.40) (Y 0.44)

$env:WID_LANGUAGE = "de"
$service = Start-Process -FilePath (Join-Path $App "win-infinit-desktop.exe") -PassThru
& $ctl --wait 20 version | Out-Null
Start-Sleep -Seconds 2
Ctl goHome monitor=0

# Two notes: one being written (Markdown source), one shown formatted.
function NewNote($x, $y, [string[]]$lines, [switch]$Formatted) {
    $before = @((& $ctl state | ConvertFrom-Json).windows | Where-Object { $_.note } | ForEach-Object { $_.hwnd })
    Ctl newNote x=$x y=$y
    $note = $null
    # The first start of the notes app in a fresh session takes a while.
    for ($i = 0; $i -lt 120 -and -not $note; $i++) {
        Start-Sleep -Milliseconds 250
        $note = (& $ctl state | ConvertFrom-Json).windows | Where-Object { $_.note -and $before -notcontains $_.hwnd } | Select-Object -First 1
    }
    if (-not $note) { throw "no note window" }
    # The service puts the note at its place right after taking it over: read it again.
    Start-Sleep -Milliseconds 1200
    $hwnd = $note.hwnd
    $note = (& $ctl state | ConvertFrom-Json).windows | Where-Object { $_.hwnd -eq $hwnd } | Select-Object -First 1
    $f = $note.frame
    & $media click ([int]($f.x + 60 * $scale)) ([int]($f.y + 70 * $scale)) left
    for ($i = 0; $i -lt $lines.Count; $i++) {
        & $media type $lines[$i]
        if ($i -lt $lines.Count - 1) { & $media key enter }
    }
    if ($Formatted) { & $media key ctrl+e }
    Start-Sleep -Milliseconds 300
    return $note
}
$noteA = NewNote (X 0.10) (Y 0.76) @("# Einkauf", "- Milch und **frisches** Brot", "- Kaffee") -Formatted
$noteB = NewNote (X 0.86) (Y 0.20) @("Termin Do 10 Uhr", "Unterlagen mitbringen")
Start-Sleep -Seconds 1

# The middle of all windows on the canvas: zooming goes there.
$rects = @((& $ctl state | ConvertFrom-Json).windows | Where-Object { $_.mode -eq "canvas" } | ForEach-Object { $_.canvasRect })
$CX = [int](((($rects | ForEach-Object { $_.x }) | Measure-Object -Minimum).Minimum + (($rects | ForEach-Object { $_.x + $_.width }) | Measure-Object -Maximum).Maximum) / 2)
$CY = [int](((($rects | ForEach-Object { $_.y }) | Measure-Object -Minimum).Minimum + (($rects | ForEach-Object { $_.y + $_.height }) | Measure-Object -Maximum).Maximum) / 2)
Note "windows around $CX,$CY"

# --- Scenes ------------------------------------------------------------------------------------
if (Wanted "notes") {
    & $media move (X 0.5) (Y 0.98)
    Start-Sleep -Milliseconds 500
    $a = $noteA.frame; $b = $noteB.frame
    Shot "notes" @([int]($a.x - 30 * $scale), [int]($a.y - 30 * $scale), [int]($a.width + 60 * $scale), [int]($a.height + 60 * $scale))
    Shot "note-editing" @([int]($b.x - 30 * $scale), [int]($b.y - 30 * $scale), [int]($b.width + 60 * $scale), [int]($b.height + 60 * $scale))
}

# The open menu (window class #32768) and what lies around it, with a margin.
function MenuShot($name, [int[]]$also) {
    $menu = @((& $media rect "#32768") -split ' ' | ForEach-Object { [int]$_ })
    if ($menu.Count -ne 4) { Note "no menu for $name"; return }
    $left = $menu[0]; $top = $menu[1]; $right = $menu[0] + $menu[2]; $bottom = $menu[1] + $menu[3]
    if ($also) {
        $left = [Math]::Min($left, $also[0]); $top = [Math]::Min($top, $also[1])
        $right = [Math]::Max($right, $also[0] + $also[2]); $bottom = [Math]::Max($bottom, $also[1] + $also[3])
    }
    $margin = [int](24 * $scale)
    Shot $name @(($left - $margin), ($top - $margin), ($right - $left + 2 * $margin), ($bottom - $top + 2 * $margin))
}

if (Wanted "menus") {
    $b = $noteB.frame
    & $media click ([int]($b.x + $b.width - 45 * $scale)) ([int]($b.y + 15 * $scale)) left
    Start-Sleep -Milliseconds 800
    MenuShot "note-menu" @([int]$b.x, [int]$b.y, [int]$b.width, [int]$b.height)
    & $media key escape
    Start-Sleep -Milliseconds 400
    & $media click (X 0.30) (Y 0.88) right 1 win
    Start-Sleep -Milliseconds 800
    MenuShot "canvas-menu"
    & $media key escape
    Start-Sleep -Milliseconds 400
}

if (Wanted "overview") {
    Ctl setCamera x=$CX y=$CY zoom=0.55 monitor=0
    Start-Sleep -Seconds 2
    & $media move (X 0.5) (Y 0.97)
    Shot "overview"
    Ctl resetZoom monitor=0
    Start-Sleep -Seconds 2
}

if (Wanted "zoom") {
    $frames = Join-Path $Out "frames-zoom"
    New-Item -ItemType Directory -Force $frames | Out-Null
    $steps = 18
    $n = 0
    foreach ($i in (0..$steps) + ($steps..0)) {
        # From the home view at 100 % to the middle of the windows at 45 %.
        $t = [Math]::Sin([Math]::PI / 2 * $i / $steps)
        $zoom = 1.0 - 0.55 * $t
        Ctl setCamera x=$([int]($CX * $t)) y=$([int]($CY * $t)) zoom=$zoom monitor=0
        Start-Sleep -Milliseconds 250
        & $media shot (Join-Path $frames ("{0:D3}.png" -f $n++))
    }
    Ctl resetZoom monitor=0
    Start-Sleep -Seconds 2
}

if (Wanted "pan") {
    # At 100 % the real windows move; each step waits for the programs to follow.
    $frames = Join-Path $Out "frames-pan"
    New-Item -ItemType Directory -Force $frames | Out-Null
    $n = 0
    $path = @()
    foreach ($i in 0..15) { $path += , @(-45, 0) }
    foreach ($i in 0..15) { $path += , @(45, 0) }
    & $media shot (Join-Path $frames ("{0:D3}.png" -f $n++))
    foreach ($step in $path) {
        Ctl pan dx=$($step[0] * $scale) dy=$($step[1] * $scale) monitor=0
        Start-Sleep -Milliseconds 350
        & $media shot (Join-Path $frames ("{0:D3}.png" -f $n++))
    }
}

if (Wanted "map") {
    & $media move (X 0.5) (Y 0.5)
    Ctl showMap
    Start-Sleep -Seconds 2
    Shot "map"
    & $media key escape
    Start-Sleep -Milliseconds 500
}

# --- Clean up ----------------------------------------------------------------------------------
Ctl quit
if (-not $service.WaitForExit(15000)) { Stop-Process -Id $service.Id -Force }
Get-Process wid-notes -ErrorAction SilentlyContinue | Where-Object { $_.Path -like "$App*" } | Stop-Process -Force
foreach ($process in $started) {
    if (-not $process.HasExited) { Stop-Process -Id $process.Id -Force }
}
Remove-Item -Recurse -Force $data -ErrorAction SilentlyContinue
Note "done"
