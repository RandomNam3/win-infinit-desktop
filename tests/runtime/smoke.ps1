# SPDX-FileCopyrightText: 2026 WIN Infinit Desktop contributors
# SPDX-License-Identifier: GPL-3.0-or-later
#
# Runtime smoke test of the canvas service on a real desktop (CI runner or a test machine):
#  1. a test window appears and the service adopts it as a canvas window,
#  2. panning by IPC really moves it, also off every monitor (parked),
#  3. quitting the service brings it back onto a screen.
# Exit code 0 = passed. Usage: pwsh tests/runtime/smoke.ps1 [-Build build]

param([string]$Build = "build")

$ErrorActionPreference = "Stop"
$service = Join-Path $Build "src/service/win-infinit-desktop.exe"
$ctl = Join-Path $Build "tools/wid-ctl/wid-ctl.exe"
$testWindow = Join-Path $Build "tools/wid-testwindow/wid-testwindow.exe"
$probe = Join-Path $Build "tools/wid-probe/wid-probe.exe"
$failures = @()

function Fail($message) {
    Write-Host "FAIL: $message"
    $script:failures += $message
}

function Handle($text) { [Convert]::ToInt64(($text -replace '^0x', ''), 16) }

function WindowState($hwnd) {
    $state = & $ctl state | ConvertFrom-Json
    $state.windows | Where-Object { (Handle $_.hwnd) -eq $hwnd } | Select-Object -First 1
}

$output = Join-Path ([IO.Path]::GetTempPath()) "wid-testwindow.txt"
$window = Start-Process -FilePath $testWindow -ArgumentList "300 300 640 400 180" -PassThru -RedirectStandardOutput $output
Start-Sleep -Seconds 1
$hwnd = Handle ((Get-Content $output -First 1).Trim())
Write-Host ("test window 0x{0:X}" -f $hwnd)

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
        Write-Host "adopted: mode $($before.mode), frame $($before.frame.x),$($before.frame.y)"
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
        Write-Host "after pan -200: frame $($moved.frame.x),$($moved.frame.y)"
        if ([Math]::Abs($dx + 200) -gt 2) { Fail "the window moved by $dx instead of -200" }

        & $ctl pan dx=-6000 dy=0 monitor=$monitor | Out-Null
        Start-Sleep -Milliseconds 700
        $parked = WindowState $hwnd
        Write-Host "after pan -6000: parked $($parked.parked), frame $($parked.frame.x),$($parked.frame.y)"
        if (-not $parked.parked) { Fail "the window was not parked" }
        $onScreen = $false
        foreach ($m in $state.monitors) {
            if ($parked.frame.x -lt $m.bounds.x + $m.bounds.width -and $parked.frame.x + $parked.frame.width -gt $m.bounds.x -and
                $parked.frame.y -lt $m.bounds.y + $m.bounds.height -and $parked.frame.y + $parked.frame.height -gt $m.bounds.y) { $onScreen = $true }
        }
        if ($onScreen) { Fail "the parked window is still on a monitor" }
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
        Write-Host "after quit: frame $($frame[0]),$($frame[1])"
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
    Write-Host "$($failures.Count) check(s) failed"
    exit 1
}
Write-Host "smoke test passed"
