# SPDX-FileCopyrightText: 2026 WIN Infinit Desktop contributors
# SPDX-License-Identifier: GPL-3.0-or-later
#
# Installs or updates WIN Infinit Desktop with one command, in PowerShell, as a normal user:
#
#   irm https://raw.githubusercontent.com/RandomNam3/win-infinit-desktop/main/install.ps1 | iex
#
# Takes the newest release from GitHub (the package for this computer, else the x64 one, which
# Windows on ARM runs too), checks its SHA-256 against the one GitHub publishes, unpacks it to
# %TEMP% and runs its installer (scripts/install.ps1 in this repository): for the current user,
# without administrator rights. The downloaded files are removed afterwards.

& {
    $ErrorActionPreference = "Stop"
    # Invoke-WebRequest is many times faster without its progress bar.
    $ProgressPreference = "SilentlyContinue"
    [Net.ServicePointManager]::SecurityProtocol = [Net.ServicePointManager]::SecurityProtocol -bor [Net.SecurityProtocolType]::Tls12

    $repository = "RandomNam3/win-infinit-desktop"
    $product = "WIN Infinit Desktop"
    $headers = @{ "User-Agent" = "win-infinit-desktop-install"; "Accept" = "application/vnd.github+json" }

    Write-Host "==> Looking for the newest release of $product"
    $release = Invoke-RestMethod -UseBasicParsing -Uri "https://api.github.com/repos/$repository/releases/latest" -Headers $headers
    $version = $release.tag_name -replace '^v', ''
    $architecture = if ($env:PROCESSOR_ARCHITECTURE -eq "ARM64") { "arm64" } else { "x64" }
    $asset = $null
    foreach ($candidate in @($architecture, "x64")) {
        $asset = $release.assets | Where-Object { $_.name -eq "win-infinit-desktop-$version-$candidate.zip" } | Select-Object -First 1
        if ($asset) { break }
    }
    if (-not $asset) {
        throw "Release $version has no package for this computer. See https://github.com/$repository/releases"
    }

    $directory = Join-Path $env:TEMP "win-infinit-desktop-install-$version"
    if (Test-Path $directory) { Remove-Item -Recurse -Force $directory }
    New-Item -ItemType Directory -Force -Path $directory | Out-Null
    try {
        $zip = Join-Path $directory "package.zip"
        Write-Host ("==> Downloading {0} ({1:N0} MB)" -f $asset.name, ($asset.size / 1MB))
        Invoke-WebRequest -UseBasicParsing -Uri $asset.browser_download_url -OutFile $zip -Headers @{ "User-Agent" = $headers["User-Agent"] }

        $digest = if ($asset.PSObject.Properties.Name -contains "digest") { [string]$asset.digest } else { "" }
        if ($digest -like "sha256:*") {
            $actual = (Get-FileHash -Algorithm SHA256 -Path $zip).Hash
            if ($actual -ne $digest.Substring(7)) {
                throw "The download is damaged: its SHA-256 does not match the one on GitHub. Nothing was installed."
            }
            Write-Host "    SHA-256 checked"
        }

        Write-Host "==> Unpacking"
        $package = Join-Path $directory "package"
        Expand-Archive -Path $zip -DestinationPath $package -Force

        $installed = Test-Path (Join-Path $env:LOCALAPPDATA "Programs\$product\win-infinit-desktop.exe")
        $arguments = @("-NoProfile", "-ExecutionPolicy", "Bypass", "-File", (Join-Path $package "install.ps1"))
        if ($installed) { $arguments += "-Update" }
        & powershell.exe @arguments
        if ($LASTEXITCODE -ne 0) {
            throw "The installer stopped with an error (see above)."
        }
    } finally {
        Remove-Item -Recurse -Force $directory -ErrorAction SilentlyContinue
    }
}
