#!/bin/bash
#
# Builds Synth-O-Matic.app.
#
#   scripts/build_app.sh [debug|release]     (default release)
#
# The app goes in build/Synth-O-Matic.app, with the original example patches
# from ../SOM-classic copied into it.  It is signed ad hoc so it runs locally.
#
set -euo pipefail

CONFIG="${1:-release}"
ROOT="$(cd "$(dirname "$0")/.." && pwd)"
CLASSIC="$ROOT/../SOM-classic"
APP="$ROOT/build/Synth-O-Matic.app"

cd "$ROOT"
swift build -c "$CONFIG" --product SynthOMatic
BIN="$(swift build -c "$CONFIG" --show-bin-path)/SynthOMatic"

rm -rf "$APP"
mkdir -p "$APP/Contents/MacOS" "$APP/Contents/Resources/Examples"
cp "$BIN" "$APP/Contents/MacOS/SynthOMatic"
cp Resources/Info.plist "$APP/Contents/Info.plist"
cp Resources/AppIcon.icns "$APP/Contents/Resources/AppIcon.icns"
printf 'APPL????' > "$APP/Contents/PkgInfo"

# the original example patches; skip Finder icon files and anything that isn't a patch
copy_examples() {
    local src="$1" dst="$APP/Contents/Resources/Examples/$2"
    [ -d "$src" ] || return 0
    mkdir -p "$dst"
    for f in "$src"/*; do
        [ -f "$f" ] || continue
        if [ "$(head -c 4 "$f")" = "SOM1" ]; then
            cp -X "$f" "$dst/"
        fi
    done
}
copy_examples "$CLASSIC/SOM patches" "Original Patches"
copy_examples "$CLASSIC/Patches/patches 1" "More Patches"

# classic Mac files carry resource forks and Finder info, which codesign refuses
xattr -cr "$APP"
codesign --force --sign - --timestamp=none "$APP"
echo "built $APP"
