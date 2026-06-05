param (
    [string]$TargetDir = "D:\deps-build",
    [string]$OutputDir = "D:\deps-output",
    [string]$QtVersion = "6.5.9",
    [string]$FFmpegVersion = "8.1.1"
)

$ErrorActionPreference = "Stop"

# 创建目录
New-Item -ItemType Directory -Force -Path $TargetDir | Out-Null
New-Item -ItemType Directory -Force -Path $OutputDir | Out-Null

$DepsDir = Join-Path $TargetDir "deps"
New-Item -ItemType Directory -Force -Path $DepsDir | Out-Null

# 1. 编译 FFmpeg
Write-Host "=== Building FFmpeg $FFmpegVersion ==="
$MsysDepsDir = $DepsDir.Replace("\", "/").Replace("D:", "/d").Replace("C:", "/c")
$TargetDirUnix = $TargetDir.Replace("\", "/").Replace("D:", "/d").Replace("C:", "/c")

# 写入临时 bash 编译脚本以在 MSYS2 环境下执行
$BashScript = @"
#!/usr/bin/env bash
set -euo pipefail

# Avoid linker clash with MSYS2 link tool
if [ -f /usr/bin/link.exe ]; then
    mv /usr/bin/link.exe /usr/bin/link-original.exe
fi

# Build nv-codec-headers for NVIDIA hardware encoding/decoding support
cd $TargetDirUnix
if [ ! -d "nv-codec-headers" ]; then
    git clone https://git.videolan.org/git/ffmpeg/nv-codec-headers.git
    cd nv-codec-headers
    git checkout n12.1.14.0
    make install PREFIX=/usr
    cd ..
fi

# Download & Extract FFmpeg Source
ffmpeg_tar="ffmpeg-${FFmpegVersion}.tar.xz"
if [ ! -d "ffmpeg-${FFmpegVersion}" ]; then
    if [ ! -f "`$ffmpeg_tar" ]; then
        curl -L -o "`$ffmpeg_tar" "https://ffmpeg.org/releases/ffmpeg-${FFmpegVersion}.tar.xz"
    fi
    tar -xf "`$ffmpeg_tar"
fi

cd "ffmpeg-${FFmpegVersion}"
./configure \
    --toolchain=msvc \
    --prefix="$MsysDepsDir/ffmpeg" \
    --enable-shared \
    --disable-static \
    --disable-doc \
    --disable-ffplay \
    --disable-avdevice \
    --enable-nvenc \
    --enable-nvdec \
    --enable-cuvid \
    --enable-ffnvcodec \
    --arch=x86_64

make -j\$(nproc)
make install

# Restore original link tool
if [ -f /usr/bin/link-original.exe ]; then
    mv /usr/bin/link-original.exe /usr/bin/link.exe
fi
"@

$BashScriptPath = Join-Path $TargetDir "build-ffmpeg.sh"
Set-Content -Path $BashScriptPath -Value $BashScript -Encoding utf8NoBOM

Write-Host "Running FFmpeg build inside MSYS2..."
& C:\msys64\usr\bin\bash.exe --login -c "$TargetDirUnix/build-ffmpeg.sh"

# 2. 编译 Qt 6.5.9
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
$OriginalLocation = Get-Location
Set-Location $QtBuildDir

$QtInstallDir = Join-Path $DepsDir "qt6"

Write-Host "Configuring Qt..."
# Configure Qt with the same modules and skip list as macOS
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

# 3. 编译 QWindowKit
Write-Host "=== Building QWindowKit ==="
$QwkSrcDir = Join-Path $TargetDir "qwindowkit-src"
if (-not (Test-Path $QwkSrcDir)) {
    Write-Host "Cloning QWindowKit..."
    git clone --recursive https://github.com/stdware/qwindowkit.git $QwkSrcDir
    Set-Location $QwkSrcDir
    git checkout 1.5.0
    git submodule update --init --recursive
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

# 4. 打包输出为 ZIP 格式
Write-Host "=== Packaging Dependencies ==="
Set-Location $DepsDir

Write-Host "Compressing ffmpeg..."
Compress-Archive -Path "ffmpeg" -DestinationPath (Join-Path $OutputDir "ffmpeg-windows-x64.zip") -Force
Write-Host "Compressing qt6..."
Compress-Archive -Path "qt6" -DestinationPath (Join-Path $OutputDir "qt6-windows-x64.zip") -Force
Write-Host "Compressing qwindowkit..."
Compress-Archive -Path "qwindowkit" -DestinationPath (Join-Path $OutputDir "qwindowkit-windows-x64.zip") -Force

Set-Location $OriginalLocation
Write-Host "=== Done ==="
