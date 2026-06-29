$ErrorActionPreference = "Stop"

# Automatically restore Linux .so symlinks to local binary copies on Windows before building
Write-Host "Restoring shared library symlinks for Windows host..."
& (Join-Path $PSScriptRoot "restore_symlinks.ps1")

function Test-Command {
    param([string]$Name)
    return $null -ne (Get-Command $Name -ErrorAction SilentlyContinue)
}

if (-not (Test-Command "docker")) {
    throw "docker is required but was not found in PATH."
}

$ProjectRoot = Resolve-Path (Join-Path $PSScriptRoot "..")
Push-Location $ProjectRoot
try {
    docker compose -f docker-compose.sophon.yml up --build
} finally {
    Pop-Location
}
