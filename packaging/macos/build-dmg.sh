#!/usr/bin/env bash
# Build an unsigned (or Developer ID-signed) macOS .app bundle and .dmg.
#
# Requirements: macOS, Xcode command line tools, CMake, Homebrew Qt 6,
# qscintilla2 (optional qtermwidget). Run from the repository root or from
# anywhere; the script locates the root itself.
#
# Usage:
#   packaging/macos/build-dmg.sh [output-directory]
#
# Environment:
#   BUILD_DIR           CMake build directory (default: build-macos)
#   OUTPUT_DIR          Directory for the .dmg (default: dist)
#   CODESIGN_IDENTITY   "Developer ID Application: Name (TEAMID)" to sign.
#                       When unset, the bundle is only ad-hoc signed.
#   NOTARIZE            Set to 1 (together with CODESIGN_IDENTITY and a stored
#                       notarytool keychain profile in NOTARY_PROFILE) to submit
#                       and staple. See packaging/macos/SIGNING.md.
#   NOTARY_PROFILE      notarytool keychain profile name (default: lite-studio)
set -euo pipefail

ROOT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")/../.." && pwd)"
cd "$ROOT_DIR"

BUILD_DIR="${BUILD_DIR:-build-macos}"
OUTPUT_DIR="${OUTPUT_DIR:-${1:-dist}}"
CODESIGN_IDENTITY="${CODESIGN_IDENTITY:-}"
NOTARIZE="${NOTARIZE:-0}"
NOTARY_PROFILE="${NOTARY_PROFILE:-lite-studio}"

if [[ "$(uname -s)" != "Darwin" ]]; then
    echo "error: this script must run on macOS" >&2
    exit 1
fi

for tool in cmake hdiutil otool install_name_tool codesign; do
    command -v "$tool" >/dev/null || {
        echo "error: required tool not found: $tool" >&2
        exit 1
    }
done

if command -v brew >/dev/null; then
    qt_prefix="$(brew --prefix qt 2>/dev/null || true)"
    if [[ -n "$qt_prefix" ]]; then
        export PATH="$qt_prefix/bin:$PATH"
    fi
fi
command -v macdeployqt >/dev/null || {
    echo "error: macdeployqt not found (brew install qt)" >&2
    exit 1
}

mkdir -p "$OUTPUT_DIR"

version="$(cmake -S . -B "$BUILD_DIR" -DCMAKE_BUILD_TYPE=Release 2>/dev/null |
    sed -n 's/^-- Lite Studio version: //p' | head -n1)"
version="${version:-0.0.0-dev}"

echo "Configuring and building Lite Studio ($version)..."
cmake -S . -B "$BUILD_DIR" \
    -DCMAKE_BUILD_TYPE=Release \
    -DCMAKE_PREFIX_PATH="$(brew --prefix qt):$(brew --prefix qscintilla2 2>/dev/null || true)"
cmake --build "$BUILD_DIR" --parallel

app="$BUILD_DIR/lite-studio.app"
if [[ ! -d "$app" ]]; then
    echo "error: expected bundle not found: $app" >&2
    exit 1
fi

echo "Bundling third-party libraries..."
frameworks="$app/Contents/Frameworks"
executable="$app/Contents/MacOS/lite-studio"
mkdir -p "$frameworks"

bundle_dylib() {
    local dylib="$1"
    [[ -f "$dylib" ]] || return 0
    local base
    base="$(basename "$dylib")"
    cp -f "$dylib" "$frameworks/$base"
    chmod u+w "$frameworks/$base"
    install_name_tool -id "@rpath/$base" "$frameworks/$base"
    install_name_tool -change "$dylib" "@rpath/$base" "$executable"
}

while IFS= read -r dependency; do
    case "$dependency" in
    /opt/homebrew/* | /usr/local/*) bundle_dylib "$dependency" ;;
    esac
done < <(otool -L "$executable" | awk 'NR > 1 { print $1 }')

echo "Deploying Qt frameworks..."
macdeployqt "$app" -always-overwrite

if ! otool -l "$executable" | grep -q "@executable_path/../Frameworks"; then
    install_name_tool -add_rpath "@executable_path/../Frameworks" "$executable"
fi

if [[ -n "$CODESIGN_IDENTITY" ]]; then
    echo "Signing with: $CODESIGN_IDENTITY"
    find "$frameworks" -type f \( -name '*.dylib' -o -perm -u+x \) -print0 |
        while IFS= read -r -d '' binary; do
            codesign --force --timestamp --options runtime \
                --sign "$CODESIGN_IDENTITY" "$binary"
        done
    find "$app/Contents" -maxdepth 2 -name '*.framework' -type d -print0 |
        while IFS= read -r -d '' framework; do
            codesign --force --timestamp --options runtime \
                --sign "$CODESIGN_IDENTITY" "$framework"
        done
    codesign --force --timestamp --options runtime \
        --entitlements packaging/macos/entitlements.plist \
        --sign "$CODESIGN_IDENTITY" "$app"
else
    echo "No CODESIGN_IDENTITY set; applying an ad-hoc signature."
    codesign --force --deep --sign - "$app"
fi

staging="$BUILD_DIR/dmg"
rm -rf "$staging"
mkdir -p "$staging"
cp -R "$app" "$staging/"
ln -s /Applications "$staging/Applications"

dmg="$OUTPUT_DIR/LiteStudio-${version}-macos-$(uname -m).dmg"
echo "Creating $dmg..."
hdiutil create -volname "Lite Studio" -srcfolder "$staging" \
    -ov -format UDZO "$dmg"

if [[ "$NOTARIZE" == "1" ]]; then
    if [[ -z "$CODESIGN_IDENTITY" ]]; then
        echo "error: NOTARIZE=1 requires CODESIGN_IDENTITY" >&2
        exit 1
    fi
    echo "Submitting for notarization (profile: $NOTARY_PROFILE)..."
    xcrun notarytool submit "$dmg" --keychain-profile "$NOTARY_PROFILE" --wait
    xcrun stapler staple "$app"
    xcrun stapler staple "$dmg"
fi

echo "Built: $dmg"
