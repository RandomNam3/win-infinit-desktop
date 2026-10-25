# SPDX-FileCopyrightText: 2026 WIN Infinit Desktop contributors
# SPDX-License-Identifier: GPL-3.0-or-later
#
# Runtime smoke test of the canvas service on a real desktop (CI runner or a test machine):
#  1. a test window appears and the service adopts it as a canvas window,
#  2. panning by IPC really moves it, also off every monitor (parked),
#  3. zooming out opens the overview without moving the real window,
#  4. a reopened window gets the place its predecessor had,
#  5. opening a group moves its window to the group's place and the camera follows,
#  6. a window announced as a note can be pinned and released over IPC,
#  7. quitting the service brings everything back onto a screen.
# Exit code 0 = passed. Usage: pwsh tests/runtime/smoke.ps1 [-Build build]

param([string]$Build = "build")

$ErrorActionPreference = "Stop"
$service = Join-Path $Build "src/service/win-infinit-desktop.exe"
$ctl = Join-Path $Build "tools/wid-ctl/wid-ctl.exe"
$testWindow = Join-Path $Build "tools/wid-testwindow/wid-testwindow.exe"
$probe = Join-Path $Build "tools/wid-probe/wid-probe.exe"
$failures = @()

$results = Join-Path (Get-Location) "smoke-result.txt"
Set-Content -Path $results -Value "smoke test $(Get-Date -Format o)"

function Note($message) {
    Write-Host $message
    Add-Content -Path $results -Value $message
}

function Fail($message) {
    Note "FAIL: $message"
    $script:failures += $message
}

function Handle($text) { [Convert]::ToInt64(($text -replace '^0x', ''), 16) }

function WindowState($hwnd) {
    $state = & $ctl state | ConvertFrom-Json
    $state.windows | Where-Object { (Handle $_.hwnd) -eq $hwnd } | Select-Object -First 1
}

# -NoNewWindow: the console program would otherwise get a terminal window of its own, which the
# service adopts and reveals like any new window (and so moves the camera).
function StartWindow($x, $y) {
    $output = Join-Path ([IO.Path]::GetTempPath()) ("wid-testwindow-{0}.txt" -f [Guid]::NewGuid())
    $process = Start-Process -FilePath $testWindow -ArgumentList "$x $y 640 400 180" -PassThru -NoNewWindow -RedirectStandardOutput $output
    for ($i = 0; $i -lt 50; $i++) {
        if ((Test-Path $output -PathType Leaf) -and (Get-Item $output).Length -gt 0) { break }
        Start-Sleep -Milliseconds 100
    }
    $handle = Handle ((Get-Content $output -First 1).Trim())
    Write-Host ("test window 0x{0:X}" -f $handle)
    return @($process, $handle)
}

function Near($a, $b) { [Math]::Abs($a - $b) -le 2 }

$window, $hwnd = StartWindow 300 300

$env:WID_DEBUG = "1"
$process = Start-Process -FilePath $service -ArgumentList "--no-tray" -PassThru
try {
    & $ctl --wait 20 version
    if ($LASTEXITCODE -ne 0) { throw "the service does not answer" }
    Start-Sleep -Seconds 1

    $before = WindowState $hwnd
    if (-not $before) {
        Fail "the test window was not adopted"
    } else {
        Note "adopted: mode $($before.mode), frame $($before.frame.x),$($before.frame.y)"
        if ($before.mode -ne "canvas") { Fail "mode is $($before.mode), expected canvas" }
        $state = & $ctl state | ConvertFrom-Json
        $monitor = 0
        for ($i = 0; $i -lt $state.monitors.Count; $i++) {
            if ($state.monitors[$i].key -eq $before.canvas) { $monitor = $i }
        }

        & $ctl pan dx=-200 dy=0 monitor=$monitor | Out-Null
        Start-Sleep -Milliseconds 700
        $moved = WindowState $hwnd
        $dx = $moved.frame.x - $before.frame.x
        Note "after pan -200: frame $($moved.frame.x),$($moved.frame.y)"
        if ([Math]::Abs($dx + 200) -gt 2) { Fail "the window moved by $dx instead of -200" }

        # Zoomed out: the overview opens, the real window stays where it is.
        & $ctl setZoom zoom=0.5 monitor=$monitor | Out-Null
        Start-Sleep -Milliseconds 700
        $state = & $ctl state | ConvertFrom-Json
        $zoomed = WindowState $hwnd
        Note "zoom 0.5: overview $($state.monitors[$monitor].overview), frame $($zoomed.frame.x),$($zoomed.frame.y)"
        if (-not $state.monitors[$monitor].overview) { Fail "no overview at 50 %" }
        if (-not (Near $zoomed.frame.x $moved.frame.x)) { Fail "the window moved when zooming out" }
        & $ctl resetZoom monitor=$monitor | Out-Null
        Start-Sleep -Milliseconds 700
        $state = & $ctl state | ConvertFrom-Json
        if ($state.monitors[$monitor].overview) { Fail "the overview stayed open at 100 %" }

        & $ctl pan dx=-6000 dy=0 monitor=$monitor | Out-Null
        Start-Sleep -Milliseconds 700
        $parked = WindowState $hwnd
        Note "after pan -6000: parked $($parked.parked), frame $($parked.frame.x),$($parked.frame.y)"
        if (-not $parked.parked) { Fail "the window was not parked" }
        $onScreen = $false
        foreach ($m in $state.monitors) {
            if ($parked.frame.x -lt $m.bounds.x + $m.bounds.width -and $parked.frame.x + $parked.frame.width -gt $m.bounds.x -and
                $parked.frame.y -lt $m.bounds.y + $m.bounds.height -and $parked.frame.y + $parked.frame.height -gt $m.bounds.y) { $onScreen = $true }
        }
        if ($onScreen) { Fail "the parked window is still on a monitor" }

        # Back again: the camera is where it started, so is the window.
        & $ctl pan dx=6200 dy=0 monitor=$monitor | Out-Null
        Start-Sleep -Milliseconds 700
        $back = WindowState $hwnd
        Note "after pan +6200: frame $($back.frame.x),$($back.frame.y)"
        if (-not (Near $back.frame.x $before.frame.x) -or -not (Near $back.frame.y $before.frame.y)) { Fail "the window did not come back to its place" }

        # Closing and opening the program again: the new window gets the old place.
        Stop-Process -Id $window.Id -Force
        Start-Sleep -Seconds 1
        $window, $hwnd = StartWindow 900 500
        Start-Sleep -Milliseconds 1500
        $reopened = WindowState $hwnd
        if (-not $reopened) {
            Fail "the reopened window was not adopted"
        } else {
            Note "reopened: frame $($reopened.frame.x),$($reopened.frame.y), canvas $($reopened.canvasRect.x),$($reopened.canvasRect.y)"
            # The place on the canvas counts: another new window (revealed) may move the camera.
            if (-not (Near $reopened.canvasRect.x $back.canvasRect.x) -or -not (Near $reopened.canvasRect.y $back.canvasRect.y)) { Fail "the reopened window did not get its remembered place (canvas $($back.canvasRect.x),$($back.canvasRect.y))" }

            # Groups: a group with this program 1500 px further right. Opening it moves the
            # window there and the camera follows, so it is on the screen again.
            $rect = $reopened.canvasRect
            $group = @{ version = 1; groups = @(@{ id = "smoke"; name = "Smoke"; x = $rect.x + 1500; y = $rect.y; layout = "manual"
                        members = @(@{ app = $reopened.app; launch = ""; geometry = @{ x = 0; y = 0; width = $rect.width; height = $rect.height } }) }) }
            $groupFile = Join-Path ([IO.Path]::GetTempPath()) "wid-smoke-groups.json"
            Set-Content -Path $groupFile -Value ($group | ConvertTo-Json -Depth 8 -Compress) -Encoding utf8
            & $ctl setGroups "json=@$groupFile" | Out-Null
            if ($LASTEXITCODE -ne 0) { Fail "setGroups failed" }
            & $ctl openGroup index=0 | Out-Null
            Start-Sleep -Milliseconds 1500
            $grouped = WindowState $hwnd
            Note "group opened: group '$($grouped.group)', canvas $($grouped.canvasRect.x),$($grouped.canvasRect.y), frame $($grouped.frame.x),$($grouped.frame.y)"
            if ($grouped.group -ne "smoke") { Fail "the window was not tagged with the group" }
            if (-not (Near $grouped.canvasRect.x ($rect.x + 1500)) -or -not (Near $grouped.canvasRect.y $rect.y)) { Fail "the window did not move to the group's place" }
            if ($grouped.parked) { Fail "the camera did not follow the group" }
            $groups = (& $ctl groups | ConvertFrom-Json) | ConvertFrom-Json
            if ($groups.groups[0].openWindows -ne 1) { Fail "groups reports $($groups.groups[0].openWindows) open windows instead of 1" }

            # Notes: the notes app announces its windows by handle; pinning goes through the
            # service. The test window stands in for a note here.
            & $ctl expectNote id=smoke-note hwnd=$hwnd | Out-Null
            if ($LASTEXITCODE -ne 0) { Fail "expectNote failed" }
            & $ctl setNotePinned id=smoke-note pinned=true keepAbove=false | Out-Null
            $note = WindowState $hwnd
            Note "note: '$($note.note)', mode $($note.mode)"
            if ($note.note -ne "smoke-note" -or $note.mode -ne "pinned") { Fail "the window did not become a pinned note" }
            & $ctl setNotePinned id=smoke-note pinned=false keepAbove=false | Out-Null
            $state = & $ctl noteState id=smoke-note | ConvertFrom-Json
            if (-not $state.open -or $state.pinned) { Fail "noteState: open $($state.open), pinned $($state.pinned)" }
        }
    }

    & $ctl quit | Out-Null
    if (-not $process.WaitForExit(15000)) { Fail "the service did not quit" }
    Start-Sleep -Milliseconds 700

    # After quitting, the window must be back on a screen.
    $report = & $probe --json | ConvertFrom-Json
    $after = $report.windows | Where-Object { (Handle $_.hwnd) -eq $hwnd } | Select-Object -First 1
    if (-not $after) {
        Fail "the test window is gone"
    } else {
        $frame = $after.frame
        Note "after quit: frame $($frame[0]),$($frame[1])"
        $back = $false
        foreach ($m in $report.monitors) {
            $b = $m.work
            if ($frame[0] -lt $b[0] + $b[2] -and $frame[0] + $frame[2] -gt $b[0] -and $frame[1] -lt $b[1] + $b[3] -and $frame[1] + $frame[3] -gt $b[1]) { $back = $true }
        }
        if (-not $back) { Fail "the window was not brought back after quitting" }
    }
} finally {
    if (-not $process.HasExited) { Stop-Process -Id $process.Id -Force }
    if (-not $window.HasExited) { Stop-Process -Id $window.Id -Force }
    $log = Join-Path $env:LOCALAPPDATA "WIN Infinit Desktop\service.log"
    if (Test-Path $log) {
        Write-Host "--- service.log"
        Get-Content $log -Tail 60
    }
}

if ($failures.Count -gt 0) {
    Note "$($failures.Count) check(s) failed"
    exit 1
}
Note "smoke test passed"
