# PowerShell script to install Windows x86_64 dependencies for QEMU
# This script downloads mingw-w64 packages directly without requiring msys2 installation
#
# Usage:
#   .\install_win_deps.ps1                    # Install to current directory
#   .\install_win_deps.ps1 -InstallDir "path" # Install to specified directory

param(
    [string]$InstallDir = "."
)

# Set error action preference to stop on errors
$ErrorActionPreference = "Stop"

# Resolve installation directory to absolute path
if ([System.IO.Path]::IsPathRooted($InstallDir)) {
    $targetDir = $InstallDir
} else {
    $targetDir = Join-Path (Get-Location) $InstallDir
}

# Create target directory if it doesn't exist
if (-not (Test-Path $targetDir)) {
    New-Item -ItemType Directory -Path $targetDir | Out-Null
}

Write-Host "Installing Windows x86_64 dependencies to: $targetDir"

# Load package database from JSON file
$packageDbPath = Join-Path $PSScriptRoot "mingw_packages.json"
if (-not (Test-Path $packageDbPath)) {
    Write-Error "Package database not found: $packageDbPath"
    exit 1
}

try {
    $packageDb = Get-Content -Path $packageDbPath -Raw | ConvertFrom-Json
    $winConfig = $packageDb.'windows-x86_64'
}
catch {
    Write-Error "Failed to parse package database: $_"
    exit 1
}

# Build packages array from JSON package database
$packages = @()
foreach ($pkg in $winConfig.packages) {
    $packages += @{
        Name = $pkg.name
        Url = "$($winConfig.mirror)/$($winConfig.prefix)/$($pkg.package)"
        FileName = $pkg.package
        ExtractPaths = $pkg.extract_paths
    }
}

# Create a temporary directory for downloads
$tempDir = "temp_deps"
if (Test-Path $tempDir) {
    Remove-Item -Path $tempDir -Recurse -Force
}
New-Item -ItemType Directory -Path $tempDir | Out-Null

# Download and extract each package
foreach ($package in $packages) {
    Write-Host "Downloading $($package.Name)..."

    $filePath = Join-Path $tempDir $package.FileName

    # Download using Invoke-WebRequest
    try {
        Invoke-WebRequest -Uri $package.Url -OutFile $filePath -UseBasicParsing
        Write-Host "Downloaded $($package.FileName)"
    }
    catch {
        Write-Error "Failed to download $($package.Name): $_"
        exit 1
    }

    # Extract specific files using tar
    foreach ($extractPath in $package.ExtractPaths) {
        Write-Host "Extracting $extractPath from $($package.FileName)..."
        try {
            Push-Location $tempDir
            & tar --extract --verbose --file=$($package.FileName) $extractPath
            if ($LASTEXITCODE -ne 0) {
                throw "tar extraction failed with exit code $LASTEXITCODE"
            }
            Pop-Location
        }
        catch {
            Pop-Location
            Write-Error "Failed to extract from $($package.FileName): $_"
            exit 1
        }
    }
}

# Copy all DLL files from mingw64/bin to target directory
Write-Host "Copying DLL files to target directory..."
$mingwBinPath = Join-Path $tempDir "mingw64\bin"
if (Test-Path $mingwBinPath) {
    try {
        Copy-Item -Path "$mingwBinPath\*" -Destination $targetDir -Recurse -Force
        Write-Host "Files copied successfully to $targetDir"
    }
    catch {
        Write-Error "Failed to copy files: $_"
        exit 1
    }
}
else {
    Write-Warning "mingw64\bin directory not found"
}

# Clean up: remove temporary directory
Write-Host "Cleaning up temporary files..."
if (Test-Path $tempDir) {
    try {
        Remove-Item -Path $tempDir -Recurse -Force
        Write-Host "Removed temporary directory"
    }
    catch {
        Write-Error "Failed to remove temporary directory: $_"
    }
}

Write-Host ""
Write-Host "SUCCESS: Windows x86_64 dependencies installation completed successfully!" -ForegroundColor Green
Write-Host "All required DLL files have been copied to: $targetDir"
