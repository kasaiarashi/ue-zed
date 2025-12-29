# Build script for ZedLink Zed Extension
# Author: Krishna Teja Mekala
# PowerShell version for Windows

$ErrorActionPreference = "Stop"

$ScriptDir = Split-Path -Parent $MyInvocation.MyCommand.Path
$ZedUnrealDir = Join-Path $ScriptDir "zed-unreal"

Write-Host "===================================" -ForegroundColor Cyan
Write-Host "Building ZedLink Zed Extension" -ForegroundColor Cyan
Write-Host "===================================" -ForegroundColor Cyan

# Check for Rust
if (-not (Get-Command cargo -ErrorAction SilentlyContinue)) {
    Write-Host "Error: Rust/Cargo not found. Install from https://rustup.rs" -ForegroundColor Red
    exit 1
}

# Build helper binary
Write-Host ""
Write-Host "[1/3] Building helper binary..." -ForegroundColor Yellow
Push-Location (Join-Path $ZedUnrealDir "zed-unreal-helper")
try {
    cargo build --release
    if ($LASTEXITCODE -ne 0) { throw "Cargo build failed" }
} finally {
    Pop-Location
}
$HelperPath = Join-Path $ZedUnrealDir "zed-unreal-helper\target\release\zed-unreal-helper.exe"
Write-Host "Helper binary built: $HelperPath" -ForegroundColor Green

# Install WASM target if needed
Write-Host ""
Write-Host "[2/3] Checking WASM target..." -ForegroundColor Yellow
$InstalledTargets = rustup target list --installed
if ($InstalledTargets -notcontains "wasm32-wasip1") {
    Write-Host "Installing wasm32-wasip1 target..." -ForegroundColor Yellow
    rustup target add wasm32-wasip1
    if ($LASTEXITCODE -ne 0) { throw "Failed to add WASM target" }
}

# Build WASM extension
Write-Host ""
Write-Host "[3/3] Building WASM extension..." -ForegroundColor Yellow
Push-Location $ZedUnrealDir
try {
    cargo build --release --target wasm32-wasip1
    if ($LASTEXITCODE -ne 0) { throw "WASM build failed" }
} finally {
    Pop-Location
}

Write-Host ""
Write-Host "===================================" -ForegroundColor Cyan
Write-Host "Build Complete!" -ForegroundColor Green
Write-Host "===================================" -ForegroundColor Cyan
Write-Host ""
Write-Host "To install in Zed:" -ForegroundColor White
Write-Host "  1. Open Zed" -ForegroundColor Gray
Write-Host "  2. Ctrl+Shift+P -> 'zed: install dev extension'" -ForegroundColor Gray
Write-Host "  3. Select: $ZedUnrealDir" -ForegroundColor Gray
Write-Host ""
