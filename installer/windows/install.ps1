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
Write-Host "Viss Language Engine v0.2.2 'Lemongrab & Lemonhope' Installer" -ForegroundColor Yellow
Write-Host ""

$installDir = "$env:LOCALAPPDATA\Programs\Viss"
New-Item -ItemType Directory -Force -Path "$installDir\libs" | Out-Null

$scriptDir = Split-Path -Parent $MyInvocation.MyCommand.Path 2>$null
if (-not $scriptDir) { $scriptDir = (Get-Location).Path }

$repoRoot = Resolve-Path "$scriptDir\..\.." -ErrorAction SilentlyContinue
if (-not $repoRoot -or -not (Test-Path "$repoRoot\viss.exe")) {
    $repoRoot = (Get-Location).Path
}

if (Test-Path "$repoRoot\viss.exe") {
    Write-Host "[*] Copying compiler binary..." -ForegroundColor Gray
    Copy-Item -Path "$repoRoot\viss.exe" -Destination "$installDir\viss.exe" -Force
    if (Test-Path "$repoRoot\viss.cmd") {
        Copy-Item -Path "$repoRoot\viss.cmd" -Destination "$installDir\viss.cmd" -Force
    }
    Write-Host "[*] Copying standard libraries..." -ForegroundColor Gray
    Copy-Item -Path "$repoRoot\libs\*" -Destination "$installDir\libs\" -Recurse -Force
} else {
    Write-Host "[*] Downloading latest Viss release payload..." -ForegroundColor Cyan
    $zipUrl = "https://github.com/Halva-developer/Viss/archive/refs/heads/main.zip"
    $zipPath = "$env:TEMP\viss_install.zip"
    Invoke-WebRequest -Uri $zipUrl -OutFile $zipPath
    Expand-Archive -Path $zipPath -DestinationPath "$env:TEMP\viss_extract" -Force
    Copy-Item -Path "$env:TEMP\viss_extract\Viss-main\libs\*" -Destination "$installDir\libs\" -Recurse -Force
    Remove-Item $zipPath -Force -ErrorAction SilentlyContinue
    Remove-Item "$env:TEMP\viss_extract" -Recurse -Force -ErrorAction SilentlyContinue
}

# Add to User PATH
$userPath = [Environment]::GetEnvironmentVariable("Path", [EnvironmentVariableTarget]::User)
if ($userPath -split ';' -notcontains $installDir) {
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

# Install VS Code extension if available
$vsix = Get-ChildItem -Path "$repoRoot\extensions\viss-vscode\*.vsix" -ErrorAction SilentlyContinue | Select-Object -First 1
if ($vsix -and (Get-Command code -ErrorAction SilentlyContinue)) {
    Write-Host "[*] Installing VS Code extension..." -ForegroundColor Gray
    & code --install-extension "$($vsix.FullName)" --force 2>$null
    Write-Host "[+] VS Code extension installed! ^_^" -ForegroundColor Green
}

Write-Host ""
Write-Host "====================================================" -ForegroundColor Green
Write-Host "  Viss Language installed successfully! :3           " -ForegroundColor Green
Write-Host "====================================================" -ForegroundColor Green
Write-Host "Verify anytime in terminal by typing:"
Write-Host "  viss -v" -ForegroundColor Cyan
Write-Host "  viss run <file.viss>" -ForegroundColor Cyan
Write-Host ""
