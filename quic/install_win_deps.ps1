# PowerShell script to install Windows dependencies for QEMU
# Automatically detects architecture (x86_64 or ARM64) and installs appropriate dependencies
#
# Usage:
#   .\install_win_deps.ps1                    # Install to current directory
#   .\install_win_deps.ps1 -InstallDir "path" # Install to specified directory

param(
    [string]$InstallDir = "."
)

# Set error action preference to stop on errors
$ErrorActionPreference = "Stop"

# Detect host CPU architecture via WMI/CIM, which reports the real
# silicon regardless of whether the current PowerShell process is
# running natively or under x64-on-ARM emulation.  Win32_Processor
# .Architecture: 9 = AMD64, 12 = ARM64.
$cpuArch = (Get-CimInstance -ClassName Win32_Processor | Select-Object -First 1).Architecture
switch ($cpuArch) {
    9  { $arch = "AMD64" }
    12 { $arch = "ARM64" }
    default {
        Write-Error "Unsupported processor architecture code: $cpuArch"
        exit 1
    }
}
Write-Host "Detected architecture: $arch"

# Determine which config to use
$configKey = ""
if ($arch -eq "AMD64") {
    Write-Host "Installing x86_64 dependencies..."
    $configKey = "windows-x86_64"
} elseif ($arch -eq "ARM64") {
    Write-Host "Installing ARM64 dependencies..."
    $configKey = "windows-aarch64"
} else {
    Write-Error "Unsupported architecture: $arch. This script supports AMD64 (x86_64) and ARM64 only."
    exit 1
}

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

Write-Host "Installing Windows dependencies to: $targetDir"

# Load package database from JSON file
$packageDbPath = Join-Path $PSScriptRoot "mingw_packages.json"
if (-not (Test-Path $packageDbPath)) {
    Write-Error "Package database not found: $packageDbPath"
    exit 1
}

try {
    $packageDb = Get-Content -Path $packageDbPath -Raw | ConvertFrom-Json
    $winConfig = $packageDb.$configKey
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

# Determine the bin directory from the package database prefix
# Extract the directory name from the prefix (e.g., "mingw/mingw64" -> "mingw64")
$prefixDir = Split-Path -Leaf $winConfig.prefix
$binPath = Join-Path $tempDir "$prefixDir\bin"

# Copy all DLL files to target directory
Write-Host "Copying DLL files to target directory..."
if (Test-Path $binPath) {
    try {
        Copy-Item -Path "$binPath\*" -Destination $targetDir -Recurse -Force
        Write-Host "Files copied successfully to $targetDir"
    }
    catch {
        Write-Error "Failed to copy files: $_"
        exit 1
    }
}
else {
    Write-Warning "Binary directory not found: $binPath"
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
Write-Host "SUCCESS: Windows dependencies installation completed successfully!" -ForegroundColor Green
Write-Host "All required DLL files have been copied to: $targetDir"
