# PowerShell script to install Windows ARM64 dependencies for QEMU
# This script replaces curl with Invoke-WebRequest

# Set error action preference to stop on errors
$ErrorActionPreference = "Stop"

Write-Host "Downloading and installing Windows ARM64 dependencies..."

# Define packages to download
$packages = @(
    @{
        Name = "glib2"
        Url = "https://repo.msys2.org/mingw/clangarm64/mingw-w64-clang-aarch64-glib2-2.84.0-1-any.pkg.tar.zst"
        FileName = "glib2.pkg.tar.zst"
        ExtractPath = "clangarm64/bin/libglib-2.0-0.dll"
    },
    @{
        Name = "winpthread"
        Url = "https://repo.msys2.org/mingw/clangarm64/mingw-w64-clang-aarch64-libwinpthread-git-12.0.0.r619.g850703ae4-1-any.pkg.tar.zst"
        FileName = "winpthread.pkg.tar.zst"
        ExtractPath = "clangarm64/bin/libwinpthread-1.dll"
    },
    @{
        Name = "zlib"
        Url = "https://repo.msys2.org/mingw/clangarm64/mingw-w64-clang-aarch64-zlib-1.3.1-1-any.pkg.tar.zst"
        FileName = "zlib.pkg.tar.zst"
        ExtractPath = "clangarm64/bin/zlib1.dll"
    },
    @{
        Name = "gettext"
        Url = "https://repo.msys2.org/mingw/clangarm64/mingw-w64-clang-aarch64-gettext-0.22.4-3-any.pkg.tar.zst"
        FileName = "gettext.pkg.tar.zst"
        ExtractPath = "clangarm64/bin/libintl-8.dll"
    },
    @{
        Name = "iconv"
        Url = "https://repo.msys2.org/mingw/clangarm64/mingw-w64-clang-aarch64-libiconv-1.18-1-any.pkg.tar.zst"
        FileName = "iconv.pkg.tar.zst"
        ExtractPath = "clangarm64/bin/libiconv-2.dll"
    },
    @{
        Name = "pcre"
        Url = "https://repo.msys2.org/mingw/clangarm64/mingw-w64-clang-aarch64-pcre2-10.45-1-any.pkg.tar.zst"
        FileName = "pcre.pkg.tar.zst"
        ExtractPath = "clangarm64/bin/libpcre2-8-0.dll"
    }
)

# Download and extract each package
foreach ($package in $packages) {
    Write-Host "Downloading $($package.Name)..."
    
    # Download using Invoke-WebRequest
    try {
        Invoke-WebRequest -Uri $package.Url -OutFile $package.FileName -UseBasicParsing
        Write-Host "Downloaded $($package.FileName)"
    }
    catch {
        Write-Error "Failed to download $($package.Name): $_"
        exit 1
    }
    
    # Extract specific file using tar
    Write-Host "Extracting $($package.ExtractPath) from $($package.FileName)..."
    try {
        & tar --extract --verbose --file=$($package.FileName) $package.ExtractPath
        if ($LASTEXITCODE -ne 0) {
            throw "tar extraction failed with exit code $LASTEXITCODE"
        }
    }
    catch {
        Write-Error "Failed to extract from $($package.FileName): $_"
        exit 1
    }
}

# Copy all DLL files from clangarm64/bin to current directory
Write-Host "Copying DLL files to current directory..."
if (Test-Path "clangarm64\bin") {
    try {
        Copy-Item -Path "clangarm64\bin\*" -Destination "." -Recurse -Force
        Write-Host "Files copied successfully"
    }
    catch {
        Write-Error "Failed to copy files: $_"
        exit 1
    }
}
else {
    Write-Warning "clangarm64\bin directory not found"
}

# Clean up: remove clangarm64 directory
Write-Host "Cleaning up clangarm64 directory..."
if (Test-Path "clangarm64") {
    try {
        Remove-Item -Path "clangarm64" -Recurse -Force
        Write-Host "Removed clangarm64 directory"
    }
    catch {
        Write-Error "Failed to remove clangarm64 directory: $_"
    }
}

# Clean up: remove downloaded .zst files
Write-Host "Cleaning up downloaded files..."
try {
    Get-ChildItem -Filter "*.zst" | Remove-Item -Force
    Write-Host "Removed .zst files"
}
catch {
    Write-Error "Failed to remove .zst files: $_"
}

Write-Host "Windows ARM64 dependencies installation completed successfully!"
