#!/bin/bash
#
# Makes a release: Synth-O-Matic.app and a Patches folder with every original
# patch that works, zipped as build/release/Synth-O-Matic-<version>.zip
#
#   SIGN_IDENTITY="Developer ID Application: ..." NOTARY_PROFILE=name scripts/make_release.sh
#
# SIGN_IDENTITY  signs with hardened runtime (default: ad hoc)
# NOTARY_PROFILE a notarytool keychain profile; if set, the app is notarized and stapled
#
# A patch "works" if it compiles and its output isn't silent.  Identical
# copies are kept once, and every patch gets a .som extension so it opens
# in the app from the Finder.
#
set -euo pipefail

ROOT="$(cd "$(dirname "$0")/.." && pwd)"
CLASSIC="$ROOT/../SOM-classic"
VERSION=$(/usr/libexec/PlistBuddy -c "Print CFBundleShortVersionString" "$ROOT/Resources/Info.plist")
OUT="$ROOT/build/release"
STAGE="$OUT/Synth-O-Matic $VERSION"

cd "$ROOT"
scripts/build_app.sh release
swift build -c release --product somrender >/dev/null
RENDER="$(swift build -c release --show-bin-path)/somrender"
APP="$ROOT/build/Synth-O-Matic.app"

if [ -n "${SIGN_IDENTITY:-}" ]; then
    codesign --force --options runtime --timestamp --sign "$SIGN_IDENTITY" "$APP"
    codesign --verify --strict "$APP"
fi

rm -rf "$OUT"
mkdir -p "$STAGE/Patches"
ditto "$APP" "$STAGE/Synth-O-Matic.app"

# the working patches
added=()
for f in "$CLASSIC/SOM patches"/* "$CLASSIC/Patches/patches 1"/*; do
    [ -f "$f" ] && [ "$(head -c 4 "$f")" = "SOM1" ] || continue
    name="$(basename "$f")"
    name="${name%.pch}"
    stats=$("$RENDER" -s "$f" 2>/dev/null) || { echo "skip (doesn't compile): $(basename "$f")"; continue; }
    if echo "$stats" | grep -q "peak 0,\|peak 0$"; then echo "skip (silent): $(basename "$f")"; continue; fi
    duplicate=""
    for g in "${added[@]+"${added[@]}"}"; do
        # files written by different versions differ in padding; compare what the app reads
        if cmp -s "$f" "$g" || [ "$("$RENDER" -s -p "$f" 2>/dev/null | sed 1d)" = "$("$RENDER" -s -p "$g" 2>/dev/null | sed 1d)" ]; then
            duplicate="$g"; break
        fi
    done
    [ -n "$duplicate" ] && continue
    [ -e "$STAGE/Patches/$name.som" ] && name="$name 2"
    cp -X "$f" "$STAGE/Patches/$name.som"
    added+=("$f")
done
echo "$(ls "$STAGE/Patches" | wc -l | tr -d ' ') patches"

if [ -n "${NOTARY_PROFILE:-}" ]; then
    ditto -c -k --keepParent "$STAGE/Synth-O-Matic.app" "$OUT/notarize.zip"
    xcrun notarytool submit "$OUT/notarize.zip" --keychain-profile "$NOTARY_PROFILE" --wait
    xcrun stapler staple "$STAGE/Synth-O-Matic.app"
    rm "$OUT/notarize.zip"
fi

xattr -cr "$STAGE/Patches"
(cd "$OUT" && ditto -c -k --keepParent "Synth-O-Matic $VERSION" "Synth-O-Matic-$VERSION.zip")
echo "made $OUT/Synth-O-Matic-$VERSION.zip"
