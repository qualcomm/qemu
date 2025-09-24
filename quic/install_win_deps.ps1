# PowerShell script to install Windows x86_64 dependencies for QEMU
# This script downloads mingw-w64 packages directly without requiring msys2 installation

# Set error action preference to stop on errors
$ErrorActionPreference = "Stop"

Write-Host "Downloading and installing Windows x86_64 dependencies..."

# Define packages to download (x86_64 versions of the same packages from the batch file)
$packages = @(
    @{
        Name = "libwinpthread"
        Url = "https://repo.msys2.org/mingw/mingw64/mingw-w64-x86_64-libwinpthread-git-12.0.0.r619.g850703ae4-1-any.pkg.tar.zst"
        FileName = "libwinpthread.pkg.tar.zst"
        ExtractPath = "mingw64/bin/libwinpthread-1.dll"
    },
    @{
        Name = "glib2"
        Url = "https://repo.msys2.org/mingw/mingw64/mingw-w64-x86_64-glib2-2.84.0-1-any.pkg.tar.zst"
        FileName = "glib2.pkg.tar.zst"
        ExtractPath = "mingw64/bin/libglib-2.0-0.dll"
    },
    @{
        Name = "pixman"
        Url = "https://repo.msys2.org/mingw/mingw64/mingw-w64-x86_64-pixman-0.44.0-1-any.pkg.tar.zst"
        FileName = "pixman.pkg.tar.zst"
        ExtractPath = "mingw64/bin/libpixman-1-0.dll"
    },
    @{
        Name = "libpng"
        Url = "https://repo.msys2.org/mingw/mingw64/mingw-w64-x86_64-libpng-1.6.44-1-any.pkg.tar.zst"
        FileName = "libpng.pkg.tar.zst"
        ExtractPath = "mingw64/bin/libpng16-16.dll"
    },
    @{
        Name = "gettext"
        Url = "https://repo.msys2.org/mingw/mingw64/mingw-w64-x86_64-gettext-0.22.4-3-any.pkg.tar.zst"
        FileName = "gettext.pkg.tar.zst"
        ExtractPath = "mingw64/bin/libintl-8.dll"
    },
    @{
        Name = "pcre2"
        Url = "https://repo.msys2.org/mingw/mingw64/mingw-w64-x86_64-pcre2-10.45-1-any.pkg.tar.zst"
        FileName = "pcre2.pkg.tar.zst"
        ExtractPath = "mingw64/bin/libpcre2-8-0.dll"
    },
    @{
        Name = "libiconv"
        Url = "https://repo.msys2.org/mingw/mingw64/mingw-w64-x86_64-libiconv-1.18-1-any.pkg.tar.zst"
        FileName = "libiconv.pkg.tar.zst"
        ExtractPath = "mingw64/bin/libiconv-2.dll"
    },
    @{
        Name = "zlib"
        Url = "https://repo.msys2.org/mingw/mingw64/mingw-w64-x86_64-zlib-1.3.1-1-any.pkg.tar.zst"
        FileName = "zlib.pkg.tar.zst"
        ExtractPath = "mingw64/bin/zlib1.dll"
    }
)

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

    # Extract specific file using tar
    Write-Host "Extracting $($package.ExtractPath) from $($package.FileName)..."
    try {
        Push-Location $tempDir
        & tar --extract --verbose --file=$($package.FileName) $package.ExtractPath
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

# Copy all DLL files from mingw64/bin to current directory
Write-Host "Copying DLL files to current directory..."
$mingwBinPath = Join-Path $tempDir "mingw64\bin"
if (Test-Path $mingwBinPath) {
    try {
        Copy-Item -Path "$mingwBinPath\*" -Destination "." -Recurse -Force
        Write-Host "Files copied successfully"
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
Write-Host "All required DLL files have been copied to the current directory."
