param (
    [string]$TargetDir = "D:\deps-build",
    [string]$OutputDir = "D:\deps-output",
    [string]$QtVersion = "6.5.9",
    [string]$FFmpegVersion = "8.1.1",
    [string]$Target = "all"
)

$ErrorActionPreference = "Stop"

# 创建目录
New-Item -ItemType Directory -Force -Path $TargetDir | Out-Null
New-Item -ItemType Directory -Force -Path $OutputDir | Out-Null

$DepsDir = Join-Path $TargetDir "deps"
New-Item -ItemType Directory -Force -Path $DepsDir | Out-Null

$OriginalLocation = Get-Location

# 1. 编译 FFmpeg
if ($Target -eq "all" -or $Target -eq "ffmpeg") {
    Write-Host "=== Building FFmpeg $FFmpegVersion ==="
    
    # Clone nv-codec-headers on host PowerShell where git is available
    $NvCodecDir = Join-Path $TargetDir "nv-codec-headers"
    if (-not (Test-Path $NvCodecDir)) {
        Write-Host "Cloning nv-codec-headers..."
        & git clone https://git.videolan.org/git/ffmpeg/nv-codec-headers.git $NvCodecDir
        Set-Location $NvCodecDir
        & git checkout n12.1.14.0
        Set-Location $OriginalLocation
    }

    $MsysDepsDir = $DepsDir.Replace("\", "/").Replace("D:", "/d").Replace("d:", "/d").Replace("C:", "/c").Replace("c:", "/c")
    $TargetDirUnix = $TargetDir.Replace("\", "/").Replace("D:", "/d").Replace("d:", "/d").Replace("C:", "/c").Replace("c:", "/c")

    # Write temporary bash script
    $BashScript = @"
#!/usr/bin/env bash
set -euo pipefail

# Avoid linker clash with MSYS2 link tool
export PKG_CONFIG_ALLOW_SYSTEM_CFLAGS=1
export PKG_CONFIG_ALLOW_SYSTEM_LIBS=1

if [ -f /usr/bin/link.exe ]; then
    mv /usr/bin/link.exe /usr/bin/link-original.exe
fi

# Install nv-codec-headers into MSYS2
cd "$TargetDirUnix/nv-codec-headers"
make install PREFIX=/usr

# Download & Extract FFmpeg Source
cd "$TargetDirUnix"
ffmpeg_tar="ffmpeg-${FFmpegVersion}.tar.xz"
if [ ! -d "ffmpeg-${FFmpegVersion}" ]; then
    if [ ! -f "`$ffmpeg_tar" ]; then
        curl -L -o "`$ffmpeg_tar" "https://ffmpeg.org/releases/ffmpeg-${FFmpegVersion}.tar.xz"
    fi
    tar -xf "`$ffmpeg_tar"
fi

# Install dependencies in MSYS2
pacman -S --noconfirm --needed mingw-w64-x86_64-x264 mingw-w64-x86_64-x265 mingw-w64-x86_64-pkgconf

# Create .lib copies of MinGW import libraries for MSVC linker (both with and without 'lib' prefix)
for lib in x264 x265; do
    src=""
    if [ -f /mingw64/lib/lib${lib}.dll.a ]; then
        src=/mingw64/lib/lib${lib}.dll.a
    elif [ -f /mingw64/lib/lib${lib}.a ]; then
        src=/mingw64/lib/lib${lib}.a
    fi
    if [ -n "$src" ]; then
        cp "$src" "/mingw64/lib/lib${lib}.lib"
        cp "$src" "/mingw64/lib/${lib}.lib"
    fi
done


cd "ffmpeg-${FFmpegVersion}"
./configure \
    --toolchain=msvc \
    --prefix="/tmp/ffmpeg-install" \
    --enable-shared \
    --disable-static \
    --disable-doc \
    --disable-ffplay \
    --disable-avdevice \
    --enable-gpl \
    --enable-libx264 \
    --enable-libx265 \
    --enable-nvenc \
    --enable-nvdec \
    --enable-cuvid \
    --enable-ffnvcodec \
    --arch=x86_64 || {
        echo "=== configure failed, printing last 200 lines of ffbuild/config.log ==="
        tail -n 200 ffbuild/config.log
        exit 1
    }

make -j$env:NUMBER_OF_PROCESSORS
make install

# Restore original link tool
if [ -f /usr/bin/link-original.exe ]; then
    mv /usr/bin/link-original.exe /usr/bin/link.exe
fi
"@

    $BashScriptPath = Join-Path $TargetDir "build-ffmpeg.sh"
    [System.IO.File]::WriteAllText($BashScriptPath, $BashScript, (New-Object System.Text.UTF8Encoding($false)))

    # Use MSYS2 location from environment or fallback to default
    $MsysLocation = $env:MSYS2_LOCATION
    if (-not $MsysLocation) {
        $MsysLocation = "C:\msys64"
    }
    $BashExe = Join-Path $MsysLocation "usr\bin\bash.exe"
    $env:MSYS2_PATH_TYPE = "inherit"
    $env:MSYSTEM = "MINGW64"
    $env:INCLUDE = "$(Join-Path $MsysLocation 'mingw64\include');$env:INCLUDE"
    $env:LIB = "$(Join-Path $MsysLocation 'mingw64\lib');$env:LIB"

    Write-Host "Running FFmpeg build inside MSYS2 at $MsysLocation..."
    & $BashExe --login -c "$TargetDirUnix/build-ffmpeg.sh"
    if ($LASTEXITCODE -ne 0) {
        throw "FFmpeg build failed inside MSYS2 with exit code $LASTEXITCODE"
    }

    # Copy FFmpeg build output from MSYS2 tmp to Target deps
    Write-Host "Copying FFmpeg output to target deps folder..."
    $MsysTmpInstall = Join-Path $MsysLocation "tmp\ffmpeg-install"
    $FfmpegTarget = Join-Path $DepsDir "ffmpeg"
    New-Item -ItemType Directory -Force -Path $FfmpegTarget | Out-Null
    Copy-Item -Path "$MsysTmpInstall\*" -Destination $FfmpegTarget -Recurse -Force

    # Copy dependent DLLs from MSYS2 mingw64/bin to target ffmpeg/bin
    $FfmpegBinTarget = Join-Path $FfmpegTarget "bin"
    Copy-Item -Path (Join-Path $MsysLocation "mingw64\bin\libx264*.dll") -Destination $FfmpegBinTarget -Force -ErrorAction SilentlyContinue
    Copy-Item -Path (Join-Path $MsysLocation "mingw64\bin\libx265*.dll") -Destination $FfmpegBinTarget -Force -ErrorAction SilentlyContinue
    Copy-Item -Path (Join-Path $MsysLocation "mingw64\bin\libwinpthread*.dll") -Destination $FfmpegBinTarget -Force -ErrorAction SilentlyContinue
    Copy-Item -Path (Join-Path $MsysLocation "mingw64\bin\libgcc_s_seh*.dll") -Destination $FfmpegBinTarget -Force -ErrorAction SilentlyContinue
    Copy-Item -Path (Join-Path $MsysLocation "mingw64\bin\libstdc++*.dll") -Destination $FfmpegBinTarget -Force -ErrorAction SilentlyContinue

    # Pack FFmpeg
    Write-Host "=== Packaging FFmpeg ==="
    Set-Location $DepsDir
    Compress-Archive -Path "ffmpeg" -DestinationPath (Join-Path $OutputDir "ffmpeg-windows-x64.zip") -Force
    Set-Location $OriginalLocation
}

# 2. 编译 Qt 6.5.9
if ($Target -eq "all" -or $Target -eq "qt6") {
    Write-Host "=== Building Qt $QtVersion ==="
    $QtTarUrl = "https://download.qt.io/official_releases/qt/6.5/$QtVersion/src/single/qt-everywhere-opensource-src-$QtVersion.tar.xz"
    $QtTarPath = Join-Path $TargetDir "qt-src.tar.xz"
    $QtSrcDirName = "qt-everywhere-src-$QtVersion"
    $QtSrcDir = Join-Path $TargetDir $QtSrcDirName

    if (-not (Test-Path $QtSrcDir)) {
        if (-not (Test-Path $QtTarPath)) {
            Write-Host "Downloading Qt $QtVersion source..."
            Invoke-WebRequest -Uri $QtTarUrl -OutFile $QtTarPath
        }
        Write-Host "Extracting Qt $QtVersion source..."
        tar.exe -xf $QtTarPath -C $TargetDir
    }

    $QtBuildDir = Join-Path $TargetDir "qt6-build"
    New-Item -ItemType Directory -Force -Path $QtBuildDir | Out-Null
    Set-Location $QtBuildDir

    $QtInstallDir = Join-Path $DepsDir "qt6"

    Write-Host "Configuring Qt..."
    & "$QtSrcDir\configure.bat" -prefix $QtInstallDir `
        -opensource -confirm-license `
        -nomake examples -nomake tests `
        -release `
        -skip qt3d `
        -skip qt5compat `
        -skip qtactiveqt `
        -skip qtcharts `
        -skip qtcoap `
        -skip qtconnectivity `
        -skip qtdatavis3d `
        -skip qtdeclarative `
        -skip qtdoc `
        -skip qtgrpc `
        -skip qthttpserver `
        -skip qtimageformats `
        -skip qtlanguageserver `
        -skip qtlocation `
        -skip qtlottie `
        -skip qtmqtt `
        -skip qtnetworkauth `
        -skip qtopcua `
        -skip qtpositioning `
        -skip qtquick3d `
        -skip qtquick3dphysics `
        -skip qtquickeffectmaker `
        -skip qtquicktimeline `
        -skip qtremoteobjects `
        -skip qtscxml `
        -skip qtsensors `
        -skip qtserialbus `
        -skip qtserialport `
        -skip qtspeech `
        -skip qtvirtualkeyboard `
        -skip qtwayland `
        -skip qtwebchannel `
        -skip qtwebengine `
        -skip qtwebsockets `
        -skip qtwebview

    Write-Host "Building Qt..."
    cmake --build . --parallel 8
    Write-Host "Installing Qt..."
    cmake --install .

    Set-Location $OriginalLocation

    # Pack Qt6
    Write-Host "=== Packaging Qt6 ==="
    Set-Location $DepsDir
    Compress-Archive -Path "qt6" -DestinationPath (Join-Path $OutputDir "qt6-windows-x64.zip") -Force
    Set-Location $OriginalLocation
}

# 3. 编译 QWindowKit
if ($Target -eq "all" -or $Target -eq "qwindowkit") {
    Write-Host "=== Building QWindowKit ==="
    $QtInstallDir = Join-Path $DepsDir "qt6"
    if (-not (Test-Path $QtInstallDir)) {
        throw "Qt6 install folder not found at $QtInstallDir. Make sure Qt6 is compiled or downloaded."
    }

    $QwkSrcDir = Join-Path $TargetDir "qwindowkit-src"
    if (-not (Test-Path $QwkSrcDir)) {
        Write-Host "Cloning QWindowKit..."
        git clone --recursive https://github.com/stdware/qwindowkit.git $QwkSrcDir
        Set-Location $QwkSrcDir
        git checkout 1.5.0
        git submodule update --init --recursive
        Set-Location $OriginalLocation
    }

    $QwkBuildDir = Join-Path $TargetDir "qwindowkit-build"
    New-Item -ItemType Directory -Force -Path $QwkBuildDir | Out-Null
    Set-Location $QwkBuildDir

    $QwkInstallDir = Join-Path $DepsDir "qwindowkit"

    Write-Host "Configuring QWindowKit..."
    cmake $QwkSrcDir -DCMAKE_PREFIX_PATH="$QtInstallDir" -DCMAKE_INSTALL_PREFIX="$QwkInstallDir" -DBUILD_SHARED_LIBS=ON
    Write-Host "Building QWindowKit..."
    cmake --build . --config Release --parallel 8
    Write-Host "Installing QWindowKit..."
    cmake --install . --config Release

    Set-Location $OriginalLocation

    # Pack QWindowKit
    Write-Host "=== Packaging QWindowKit ==="
    Set-Location $DepsDir
    Compress-Archive -Path "qwindowkit" -DestinationPath (Join-Path $OutputDir "qwindowkit-windows-x64.zip") -Force
    Set-Location $OriginalLocation
}

Write-Host "=== Done ==="
