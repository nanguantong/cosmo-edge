$ErrorActionPreference = "Stop"

# ---------------------------------------------------------------------------
# Restore Linux .so symlinks that Git on Windows checks out as plain text files.
# Scans prebuild/ and 3rd/ for small files whose content is a relative path
# pointing to another file in the same directory, then replaces the placeholder
# with a copy of the real binary (recursively resolving symlink chains).
# ---------------------------------------------------------------------------

$targetDirs = @(
    "$PSScriptRoot/../prebuild",
    "$PSScriptRoot/../3rd"
)

foreach ($dir in $targetDirs) {
    $resolvedDir = [System.IO.Path]::GetFullPath($dir)
    if (-not (Test-Path $resolvedDir)) {
        Write-Warning "Directory not found: $resolvedDir"
        continue
    }

    Write-Host "Scanning for dummy symbolic link files in $resolvedDir..."

    Get-ChildItem -Path $resolvedDir -Recurse -File | ForEach-Object {
        if ($_.Length -lt 200 -and ($_.Name -like "*.so" -or $_.Name -like "*.so.*" -or $_.Extension -eq "")) {
            try {
                $content = (Get-Content $_.FullName -Raw -ErrorAction Stop).Trim()
                if ($content -and -not ($content -match "[\r\n]") -and (Test-Path (Join-Path $_.DirectoryName $content))) {
                    $targetPath = Join-Path $_.DirectoryName $content

                    # Recursively resolve symlink chain if the target itself is another placeholder
                    $safetyCounter = 0
                    while ($safetyCounter -lt 10) {
                        $targetItem = Get-Item $targetPath
                        if ($targetItem.Length -lt 200) {
                            $nextContent = (Get-Content $targetPath -Raw).Trim()
                            if ($nextContent -and -not ($nextContent -match "[\r\n]") -and (Test-Path (Join-Path $targetItem.DirectoryName $nextContent))) {
                                $targetPath = Join-Path $targetItem.DirectoryName $nextContent
                                $safetyCounter++
                                continue
                            }
                        }
                        break
                    }

                    Write-Host "Restoring: $($_.FullName) -> $targetPath"
                    Remove-Item $_.FullName -Force
                    Copy-Item -Path $targetPath -Destination $_.FullName -Force
                }
            } catch {
                # Not a plain text file or failed to read, skip
            }
        }
    }
}

Write-Host "Symlinks restoration completed successfully!"

# ---------------------------------------------------------------------------
# Build the Sophon release package
# ---------------------------------------------------------------------------

$ProjectRoot = Resolve-Path (Join-Path $PSScriptRoot "..")
Push-Location $ProjectRoot
try {
    docker compose -f docker-compose.sophon.yml up --build
} finally {
    Pop-Location
}
