#!/usr/bin/env bash
# merge-universal-macos.sh — Merge arm64 and x64 App bundles into a Universal 2 bundle
set -euo pipefail

if [[ $# -lt 3 ]]; then
    echo "Usage: $0 <arm64_app_dir> <x64_app_dir> <output_app_dir>"
    exit 1
fi

ARM64_APP="$1"
X64_APP="$2"
OUT_APP="$3"

# Verify input directories exist
if [[ ! -d "$ARM64_APP" ]]; then
    echo "Error: Arm64 App directory not found at $ARM64_APP" >&2
    exit 1
fi

if [[ ! -d "$X64_APP" ]]; then
    echo "Error: X64 App directory not found at $X64_APP" >&2
    exit 1
fi

echo "=== Cleaning output directory ==="
rm -rf "$OUT_APP"
mkdir -p "$OUT_APP"

echo "=== Copying structure and non-binary files from arm64 app ==="
cp -R "$ARM64_APP/" "$OUT_APP/"

echo "=== Merging binaries with lipo ==="
# Find all files in the output app bundle
find "$OUT_APP" -type f | while read -r out_file; do
    # Get relative path
    rel_path="${out_file#$OUT_APP/}"
    arm64_file="$ARM64_APP/$rel_path"
    x64_file="$X64_APP/$rel_path"

    # Skip if file does not exist in x64 app (e.g. metadata that differs or architecture-specific extra files)
    if [[ ! -f "$x64_file" ]]; then
        continue
    fi

    # Skip symbolic links (handled by directory copying)
    if [[ -L "$out_file" ]]; then
        continue
    fi

    # Check if the file is a Mach-O binary
    file_info=$(file -b "$out_file")
    if [[ "$file_info" == *"Mach-O"* ]]; then
        # Double check x64 is also Mach-O
        x64_file_info=$(file -b "$x64_file")
        if [[ "$x64_file_info" == *"Mach-O"* ]]; then
            echo "Merging: $rel_path"
            tmp_out=$(mktemp)
            lipo -create "$arm64_file" "$x64_file" -output "$tmp_out"
            mv "$tmp_out" "$out_file"
            chmod --reference="$arm64_file" "$out_file"
        fi
    fi
done

echo "=== Re-signing the Universal App ==="
codesign --force --deep --sign - "$OUT_APP"

echo "=== Universal App Merged Successfully ==="
ls -lh "$OUT_APP/Contents/MacOS/subtitles-editor"
file "$OUT_APP/Contents/MacOS/subtitles-editor"
