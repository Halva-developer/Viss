# =============================================================================
# Viss Language One-Liner PowerShell Installer for Windows
# Usage: irm https://raw.githubusercontent.com/Halva-developer/Viss/main/installer/windows/install.ps1 | iex
# =============================================================================

[Net.ServicePointManager]::SecurityProtocol = [Net.SecurityProtocolType]::Tls12
$ErrorActionPreference = "Stop"

Write-Host "  __      ___             " -ForegroundColor Cyan
Write-Host "  \ \    / (_)            " -ForegroundColor Cyan
Write-Host "   \ \  / / _ ___ ___     " -ForegroundColor Cyan
Write-Host "    \ \/ / | / __/ __|    " -ForegroundColor Cyan
Write-Host "     \  /  | \__ \__ \    " -ForegroundColor Cyan
Write-Host "      \/   |_|___/___/    " -ForegroundColor Cyan
Write-Host ""
Write-Host "Viss Language Engine v0.3.3 'Flame Princess' Installer" -ForegroundColor Yellow
Write-Host ""

$installDir = "$env:LOCALAPPDATA\Programs\Viss"
New-Item -ItemType Directory -Force -Path "$installDir\libs" | Out-Null

$scriptDir = $null
if ($MyInvocation.MyCommand -and $MyInvocation.MyCommand.Path) {
    $scriptDir = Split-Path -Parent $MyInvocation.MyCommand.Path
}

$repoRoot = $null
if ($scriptDir) {
    $candidate = Resolve-Path "$scriptDir\..\.." -ErrorAction SilentlyContinue
    if ($candidate -and (Test-Path "$candidate\viss.exe")) {
        $repoRoot = $candidate.Path
    }
}
if (-not $repoRoot -and (Test-Path ".\viss.exe")) {
    $repoRoot = (Get-Location).Path
}

if ($repoRoot -and (Test-Path "$repoRoot\viss.exe")) {
    Write-Host "[*] Copying compiler and runtime binaries from local repository..." -ForegroundColor Gray
    Copy-Item -Path "$repoRoot\viss.exe" -Destination "$installDir\viss.exe" -Force
    if (Test-Path "$repoRoot\vsc.exe") {
        Copy-Item -Path "$repoRoot\vsc.exe" -Destination "$installDir\vsc.exe" -Force
    }
    if (Test-Path "$repoRoot\ssi.exe") {
        Copy-Item -Path "$repoRoot\ssi.exe" -Destination "$installDir\ssi.exe" -Force
    } elseif (Test-Path "$installDir\vsc.exe") {
        Copy-Item -Path "$installDir\vsc.exe" -Destination "$installDir\ssi.exe" -Force
    }
    if (Test-Path "$repoRoot\viss.cmd") {
        Copy-Item -Path "$repoRoot\viss.cmd" -Destination "$installDir\viss.cmd" -Force
    }
    Write-Host "[*] Copying standard libraries..." -ForegroundColor Gray
    Copy-Item -Path "$repoRoot\libs\*" -Destination "$installDir\libs\" -Recurse -Force

    # Install VS Code extension if available locally
    $vsix = Get-ChildItem -Path "$repoRoot\extensions\viss-vscode\*.vsix" -ErrorAction SilentlyContinue | Sort-Object Name -Descending | Select-Object -First 1
    if ($vsix -and (Get-Command code -ErrorAction SilentlyContinue)) {
        Write-Host "[*] Installing VS Code extension ($($vsix.Name))..." -ForegroundColor Gray
        & code --install-extension "$($vsix.FullName)" --force 2>$null
        Write-Host "[+] VS Code extension installed! ^_^" -ForegroundColor Green
    }
} else {
    Write-Host "[*] Downloading latest Viss compiler and runtime binaries..." -ForegroundColor Cyan
    $binUrl = "https://github.com/Halva-developer/Viss/releases/latest/download/viss.exe"
    $vscUrl = "https://github.com/Halva-developer/Viss/releases/latest/download/vsc.exe"
    try {
        Invoke-WebRequest -Uri $binUrl -OutFile "$installDir\viss.exe" -UseBasicParsing
    } catch {
        Write-Host "[!] Note: Could not download prebuilt viss.exe from releases: $_" -ForegroundColor Yellow
    }
    try {
        Invoke-WebRequest -Uri $vscUrl -OutFile "$installDir\vsc.exe" -UseBasicParsing
        Copy-Item -Path "$installDir\vsc.exe" -Destination "$installDir\ssi.exe" -Force
    } catch {}

    Write-Host "[*] Downloading latest Viss standard library..." -ForegroundColor Cyan
    $zipUrl = "https://github.com/Halva-developer/Viss/archive/refs/heads/main.zip"
    $zipPath = "$env:TEMP\viss_install.zip"
    $extractPath = "$env:TEMP\viss_extract"
    Remove-Item $extractPath -Recurse -Force -ErrorAction SilentlyContinue

    Invoke-WebRequest -Uri $zipUrl -OutFile $zipPath -UseBasicParsing
    Expand-Archive -Path $zipPath -DestinationPath $extractPath -Force

    if (Test-Path "$extractPath\Viss-main\libs") {
        Copy-Item -Path "$extractPath\Viss-main\libs\*" -Destination "$installDir\libs\" -Recurse -Force
    }
    if (Test-Path "$extractPath\Viss-main\viss.cmd") {
        Copy-Item -Path "$extractPath\Viss-main\viss.cmd" -Destination "$installDir\viss.cmd" -Force
    }

    # Install VS Code extension if available in downloaded archive
    $downloadedVsix = Get-ChildItem -Path "$extractPath\Viss-main\extensions\viss-vscode\*.vsix" -ErrorAction SilentlyContinue | Sort-Object Name -Descending | Select-Object -First 1
    if ($downloadedVsix -and (Get-Command code -ErrorAction SilentlyContinue)) {
        Write-Host "[*] Installing VS Code extension ($($downloadedVsix.Name))..." -ForegroundColor Gray
        & code --install-extension "$($downloadedVsix.FullName)" --force 2>$null
        Write-Host "[+] VS Code extension installed! ^_^" -ForegroundColor Green
    }

    Remove-Item $zipPath -Force -ErrorAction SilentlyContinue
    Remove-Item $extractPath -Recurse -Force -ErrorAction SilentlyContinue
}

if (-not (Test-Path "$installDir\viss.exe")) {
    Write-Host "[-] Error: viss.exe is missing in $installDir" -ForegroundColor Red
    return
}

# Add to User PATH
$userPath = [Environment]::GetEnvironmentVariable("Path", [EnvironmentVariableTarget]::User)
if (-not $userPath) {
    [Environment]::SetEnvironmentVariable("Path", $installDir, [EnvironmentVariableTarget]::User)
    $env:Path = "$installDir;$env:Path"
    Write-Host "[+] Added $installDir to User PATH." -ForegroundColor Green
} elseif ($userPath -split ';' -notcontains $installDir) {
    $newPath = "$installDir;$userPath".TrimEnd(';')
    [Environment]::SetEnvironmentVariable("Path", $newPath, [EnvironmentVariableTarget]::User)
    $env:Path = "$installDir;$env:Path"
    Write-Host "[+] Added $installDir to User PATH." -ForegroundColor Green
} else {
    Write-Host "[*] $installDir is already in User PATH." -ForegroundColor Cyan
}

# File association for .viss
try {
    New-Item -Path "HKCU:\Software\Classes\.viss" -Value "VissSourceFile" -Force | Out-Null
    New-Item -Path "HKCU:\Software\Classes\VissSourceFile" -Value "Viss Source File" -Force | Out-Null
    New-Item -Path "HKCU:\Software\Classes\VissSourceFile\shell\open\command" -Value "`"$installDir\viss.exe`" run `"%1`"" -Force | Out-Null
    New-Item -Path "HKCU:\Software\Classes\VissSourceFile\shell\Run with Viss\command" -Value "`"$installDir\viss.exe`" run `"%1`"" -Force | Out-Null
    Write-Host "[+] Registered .viss shell associations." -ForegroundColor Green
} catch {
    Write-Host "[!] Note: Could not register registry shell associations: $_" -ForegroundColor Yellow
}

Write-Host ""
Write-Host "====================================================" -ForegroundColor Green
Write-Host "  Viss Language installed successfully! :3           " -ForegroundColor Green
Write-Host "====================================================" -ForegroundColor Green
Write-Host "Verify anytime in terminal by typing:"
Write-Host "  viss -v" -ForegroundColor Cyan
Write-Host "  vsc -v" -ForegroundColor Cyan
Write-Host "  viss run <file.viss>" -ForegroundColor Cyan
Write-Host ""
