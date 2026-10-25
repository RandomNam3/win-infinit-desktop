# SPDX-FileCopyrightText: 2026 WIN Infinit Desktop contributors
# SPDX-License-Identifier: GPL-3.0-or-later
#
# Installs WIN Infinit Desktop for the current user (no administrator rights needed):
#   %LOCALAPPDATA%\Programs\WIN Infinit Desktop   programs
#   Start menu                                     "WIN Infinit Desktop", "... Settings"
#   HKCU ...\Run                                   start at login
#   HKCU ...\Uninstall                             entry in Settings > Apps
# Settings, notes and the canvas state stay in %APPDATA% / %LOCALAPPDATA%\WIN Infinit Desktop.
#
# Usage (from the extracted release zip):
#   powershell -NoProfile -ExecutionPolicy Bypass -File install.ps1 [-Update] [-NoStart]
#   powershell -NoProfile -ExecutionPolicy Bypass -File install.ps1 -Uninstall [-RemoveData]
#   -DryRun prints what would happen.
#
# A running service is asked to quit over its pipe, so every window comes back onto a
# screen before the files are replaced; it is never killed.

param(
    [switch]$Update,
    [switch]$Uninstall,
    [switch]$RemoveData,
    [switch]$NoStart,
    [switch]$DryRun
)

$ErrorActionPreference = "Stop"
Set-StrictMode -Version 3

$product = "WIN Infinit Desktop"
$target = Join-Path $env:LOCALAPPDATA "Programs\$product"
$runKey = "HKCU:\Software\Microsoft\Windows\CurrentVersion\Run"
$uninstallKey = "HKCU:\Software\Microsoft\Windows\CurrentVersion\Uninstall\WinInfinitDesktop"
$startMenu = Join-Path ([Environment]::GetFolderPath("Programs")) $product
$programs = @("win-infinit-desktop.exe", "wid-notes.exe", "wid-settings.exe", "wid-ctl.exe", "wid-probe.exe")
$minimumBuild = 26100

function Step($message) { Write-Host "==> $message" }
function Act([scriptblock]$action, $description) {
    if ($DryRun) { Write-Host "    (dry run) $description" } else { & $action }
}

# ---- Pipes (same names as src/shared/pipenames.h) ----------------------------------------
function PipeName([string]$suffix) {
    $session = (Get-Process -Id $PID).SessionId
    return "win-infinit-desktop-$session$suffix"
}

# Sends one request; true if the program answered.
function SendRequest([string]$pipe, [string]$method) {
    $client = New-Object System.IO.Pipes.NamedPipeClientStream(".", $pipe, [System.IO.Pipes.PipeDirection]::InOut)
    try {
        $client.Connect(1500)
        $writer = New-Object System.IO.StreamWriter($client)
        $writer.AutoFlush = $true
        $reader = New-Object System.IO.StreamReader($client)
        $writer.WriteLine((@{ id = 1; method = $method; params = @{} } | ConvertTo-Json -Compress))
        $task = $reader.ReadLineAsync()
        [void]$task.Wait(3000)
        return $true
    } catch {
        return $false
    } finally {
        $client.Dispose()
    }
}

function ProcessesIn([string]$directory) {
    Get-Process -ErrorAction SilentlyContinue | Where-Object {
        try { $_.Path -and $_.Path.StartsWith($directory, [StringComparison]::OrdinalIgnoreCase) } catch { $false }
    }
}

function StopPrograms {
    $service = Get-Process -Name "win-infinit-desktop" -ErrorAction SilentlyContinue
    if ($service) {
        Step "Quitting the running service (windows come back onto the screens)"
        Act {
            if (SendRequest (PipeName "") "quit") {
                $service | ForEach-Object { [void]$_.WaitForExit(20000) }
            }
            if (Get-Process -Name "win-infinit-desktop" -ErrorAction SilentlyContinue) {
                throw "The service did not quit. Quit it from its tray icon and run the installer again."
            }
        } "quit win-infinit-desktop.exe over its pipe"
    }
    if (Get-Process -Name "wid-notes" -ErrorAction SilentlyContinue) {
        Step "Quitting the notes app (it saves every note first)"
        Act {
            [void](SendRequest (PipeName "-notes") "quit")
            Get-Process -Name "wid-notes" -ErrorAction SilentlyContinue | ForEach-Object {
                if (-not $_.WaitForExit(10000)) { Stop-Process -Id $_.Id -Force }
            }
        } "quit wid-notes.exe over its pipe"
    }
    # The settings app applies every change immediately: nothing to save.
    Get-Process -Name "wid-settings" -ErrorAction SilentlyContinue | ForEach-Object {
        Act { Stop-Process -Id $_.Id -Force; [void]$_.WaitForExit(5000) } "stop wid-settings.exe"
    }
    $left = @(ProcessesIn $target)
    if ($left.Count -gt 0 -and -not $DryRun) {
        throw "Still running from $target`: $($left.Name -join ', '). Close them and run the installer again."
    }
}

function Shortcut([string]$path, [string]$exe, [string]$description) {
    $shell = New-Object -ComObject WScript.Shell
    $link = $shell.CreateShortcut($path)
    $link.TargetPath = $exe
    $link.WorkingDirectory = Split-Path $exe
    $link.Description = $description
    $link.IconLocation = "$exe,0"
    $link.Save()
}

# ---- Uninstall ---------------------------------------------------------------------------
if ($Uninstall) {
    StopPrograms
    Step "Removing $product"
    Act {
        Remove-ItemProperty -Path $runKey -Name $product -ErrorAction SilentlyContinue
        Remove-Item -Path $uninstallKey -Recurse -ErrorAction SilentlyContinue
        Remove-Item -Path $startMenu -Recurse -Force -ErrorAction SilentlyContinue
        # This script may run from the folder that is removed.
        Set-Location $env:TEMP
        Remove-Item -Path $target -Recurse -Force -ErrorAction SilentlyContinue
    } "remove the autostart entry, the Start menu folder, the uninstall entry and $target"
    if ($RemoveData) {
        Act {
            Remove-Item -Path (Join-Path $env:APPDATA $product) -Recurse -Force -ErrorAction SilentlyContinue
            Remove-Item -Path (Join-Path $env:LOCALAPPDATA $product) -Recurse -Force -ErrorAction SilentlyContinue
        } "remove settings, notes and the canvas state"
    } else {
        Write-Host "    Settings, notes and the canvas state were kept (-RemoveData removes them)."
    }
    Step "Done"
    exit 0
}

# ---- Install / update --------------------------------------------------------------------
$source = $PSScriptRoot
if (-not (Test-Path (Join-Path $source "win-infinit-desktop.exe"))) {
    throw "win-infinit-desktop.exe is not next to install.ps1: run the script from the extracted release zip."
}
if ((Resolve-Path $source).Path.TrimEnd('\') -ieq $target.TrimEnd('\')) {
    throw "Run install.ps1 from the extracted release zip, not from $target."
}
$build = [Environment]::OSVersion.Version.Build
if ($build -lt $minimumBuild) {
    throw "$product needs Windows 11 24H2 or newer (build $minimumBuild); this is build $build."
}
$version = (Get-Item (Join-Path $source "win-infinit-desktop.exe")).VersionInfo.ProductVersion
if (-not $version) { $version = "0.0.0" }
Step "$(if ($Update) { 'Updating to' } else { 'Installing' }) $product $version for $env:USERNAME"

StopPrograms

Step "Copying the programs to $target"
Act {
    New-Item -ItemType Directory -Force -Path $target | Out-Null
    # Old files go first, so a removed Qt plugin or QML module does not linger.
    Get-ChildItem -Path $target -Force | Remove-Item -Recurse -Force
    Get-ChildItem -Path $source -Force | Copy-Item -Destination $target -Recurse -Force
    # Files from a downloaded zip carry the "from the internet" mark.
    Get-ChildItem -Path $target -Recurse -File | Unblock-File
} "replace the contents of $target with the files of this package"

$service = Join-Path $target "win-infinit-desktop.exe"
$settings = Join-Path $target "wid-settings.exe"

Step "Start menu, autostart and the uninstall entry"
Act {
    New-Item -ItemType Directory -Force -Path $startMenu | Out-Null
    Shortcut (Join-Path $startMenu "$product.lnk") $service "Infinite canvas for the desktop"
    if (Test-Path $settings) {
        Shortcut (Join-Path $startMenu "$product Settings.lnk") $settings "Settings of $product"
    }
    New-ItemProperty -Path $runKey -Name $product -Value "`"$service`"" -PropertyType String -Force | Out-Null
    New-Item -Path $uninstallKey -Force | Out-Null
    $size = [int]((Get-ChildItem -Path $target -Recurse -File | Measure-Object -Property Length -Sum).Sum / 1KB)
    $values = @{
        DisplayName = $product
        DisplayVersion = $version
        Publisher = "WIN Infinit Desktop contributors"
        DisplayIcon = "`"$service`",0"
        InstallLocation = $target
        UninstallString = "powershell.exe -NoProfile -ExecutionPolicy Bypass -File `"$target\install.ps1`" -Uninstall"
        URLInfoAbout = "https://github.com/RandomNam3/win-infinit-desktop"
    }
    foreach ($name in $values.Keys) {
        New-ItemProperty -Path $uninstallKey -Name $name -Value $values[$name] -PropertyType String -Force | Out-Null
    }
    New-ItemProperty -Path $uninstallKey -Name EstimatedSize -Value $size -PropertyType DWord -Force | Out-Null
    New-ItemProperty -Path $uninstallKey -Name NoModify -Value 1 -PropertyType DWord -Force | Out-Null
    New-ItemProperty -Path $uninstallKey -Name NoRepair -Value 1 -PropertyType DWord -Force | Out-Null
} "create the Start menu entries, the Run entry and the Apps entry"

if (-not $NoStart) {
    Step "Starting $product"
    Act {
        try {
            Start-Process -FilePath $service -WorkingDirectory $target
        } catch {
            Write-Warning "The service did not start: $($_.Exception.Message)"
            Write-Warning "If Smart App Control blocks unsigned programs, see docs/INSTALL.md (troubleshooting)."
        }
    } "start $service"
}
Step "Done. The tray icon of $product has the menu; Settings are in the Start menu."
