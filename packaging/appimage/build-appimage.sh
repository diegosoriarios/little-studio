#!/usr/bin/env bash
# Build a portable AppImage for Lite Studio.
#
# Requirements: cmake, ninja (optional), the project's Qt 6 / QScintilla build
# dependencies, curl, and a Linux x86_64 (or aarch64) host. linuxdeploy and
# appimagetool are downloaded into .tools/ on first use and are not installed
# system-wide.
#
# Usage:
#   packaging/appimage/build-appimage.sh [output-directory]
#
# Environment:
#   ARCH                 Target architecture (default: uname -m)
#   LINUXDEPLOY          Path to an existing linuxdeploy binary
#   LINUXDEPLOY_QT_PLUGIN Path to an existing linuxdeploy Qt plugin
#   APPIMAGETOOL         Path to an existing appimagetool binary
#   BUILD_DIR            CMake build directory (default: build-appimage)
#   OUTPUT_DIR           Directory for the resulting .AppImage
set -euo pipefail

ROOT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")/../.." && pwd)"
cd "$ROOT_DIR"

ARCH="${ARCH:-$(uname -m)}"
BUILD_DIR="${BUILD_DIR:-build-appimage}"
OUTPUT_DIR="${OUTPUT_DIR:-${1:-dist}}"
TOOLS_DIR="${TOOLS_DIR:-$ROOT_DIR/.tools}"

# AppImage tools are themselves AppImages; extraction keeps this working on
# hosts without libfuse2 (containers, Fedora, CI runners).
export APPIMAGE_EXTRACT_AND_RUN=1

case "$ARCH" in
x86_64) APPIMAGE_ARCH=x86_64 ;;
aarch64 | arm64) APPIMAGE_ARCH=aarch64; ARCH=aarch64 ;;
*)
    echo "error: unsupported architecture: $ARCH" >&2
    exit 1
    ;;
esac

mkdir -p "$TOOLS_DIR" "$OUTPUT_DIR"

if [[ "$BUILD_DIR" = /* ]]; then
    build_abs="$BUILD_DIR"
else
    build_abs="$ROOT_DIR/$BUILD_DIR"
fi

version="$(cmake -S . -B "$build_abs" -DCMAKE_BUILD_TYPE=Release 2>/dev/null |
    sed -n 's/^-- Lite Studio version: //p' | head -n1)"
version="${version:-0.0.0-dev}"
# AppImage filenames should not contain spaces or slashes.
version_slug="$(printf '%s' "$version" | tr '/ ' '--')"

fetch() {
    local url="$1" dest="$2"
    if [[ ! -x "$dest" ]]; then
        echo "Downloading $(basename "$dest")..."
        curl -fL --retry 3 -o "$dest" "$url"
        chmod +x "$dest"
    fi
}

linuxdeploy="${LINUXDEPLOY:-$TOOLS_DIR/linuxdeploy-$APPIMAGE_ARCH.AppImage}"
qt_plugin="${LINUXDEPLOY_QT_PLUGIN:-$TOOLS_DIR/linuxdeploy-plugin-qt-$APPIMAGE_ARCH.AppImage}"
appimagetool="${APPIMAGETOOL:-$TOOLS_DIR/appimagetool-$APPIMAGE_ARCH.AppImage}"

fetch "https://github.com/linuxdeploy/linuxdeploy/releases/download/continuous/linuxdeploy-${APPIMAGE_ARCH}.AppImage" "$linuxdeploy"
fetch "https://github.com/linuxdeploy/linuxdeploy-plugin-qt/releases/download/continuous/linuxdeploy-plugin-qt-${APPIMAGE_ARCH}.AppImage" "$qt_plugin"
fetch "https://github.com/AppImage/appimagetool/releases/download/continuous/appimagetool-${APPIMAGE_ARCH}.AppImage" "$appimagetool"

echo "Configuring and building Lite Studio ($version)..."
cmake -S . -B "$build_abs" \
    -DCMAKE_BUILD_TYPE=Release \
    -DCMAKE_INSTALL_PREFIX=/usr
cmake --build "$build_abs" --parallel

echo "Staging AppDir..."
app_dir="$build_abs/AppDir"
rm -rf "$app_dir"
DESTDIR="$app_dir" cmake --install "$build_abs"

# The installed binary's RUNPATH no longer points at a project-local .deps/
# copy of QScintilla/qtermwidget. Copy every SONAME variant into the AppDir so
# both linuxdeploy and the AppImage runtime can resolve them.
mkdir -p "$app_dir/usr/lib"
for cache_var in QSCINTILLA_LIBRARY QTERMWIDGET_LIBRARY; do
    lib="$(sed -n "s/^${cache_var}:FILEPATH=//p" "$build_abs/CMakeCache.txt")"
    [[ -n "$lib" && -e "$lib" ]] || continue
    cp -a "$(dirname "$lib")"/"$(basename "$lib")"* "$app_dir/usr/lib/" 2>/dev/null || true
done

# AppImage wants the desktop file and icon at the AppDir root.
install -Dm644 "$app_dir/usr/share/applications/lite-studio.desktop" \
    "$app_dir/lite-studio.desktop"
install -Dm644 "$app_dir/usr/share/icons/hicolor/scalable/apps/lite-studio.svg" \
    "$app_dir/lite-studio.svg"

echo "Bundling libraries with linuxdeploy..."
export QMAKE
QMAKE="$(command -v qmake6 || command -v qmake)"
export OUTPUT="$OUTPUT_DIR/LiteStudio-${version_slug}-${APPIMAGE_ARCH}.AppImage"
export NO_STRIP=1
export LD_LIBRARY_PATH="$app_dir/usr/lib${LD_LIBRARY_PATH:+:$LD_LIBRARY_PATH}"
"$linuxdeploy" --appdir "$app_dir" \
    --plugin qt \
    --desktop-file "$app_dir/lite-studio.desktop" \
    --icon-file "$app_dir/lite-studio.svg" \
    --executable "$app_dir/usr/bin/lite-studio"
echo "Packaging AppImage..."
"$appimagetool" --appimage-extract-and-run --no-appstream "$app_dir" "$OUTPUT"

echo "Built: $OUTPUT"
