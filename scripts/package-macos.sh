#!/bin/bash
set -euo pipefail
cd "$(dirname "$0")/.."
[[ "$(uname -s)" == Darwin ]] || { echo 'macOS required' >&2; exit 1; }
products="$PWD/build-macos/ZaZampler_artefacts/Release"
# A fresh temporary staging directory prevents stale bundles entering the ZIP.
stage="$(mktemp -d "${TMPDIR:-/tmp}/zazampler-package.XXXXXX")"
trap 'rm -rf "$stage"' EXIT
package="$stage/ZaZampler-macOS-arm64"
mkdir -p "$package/scripts" "$PWD/dist"
for relative in 'VST3/ZaZampler.vst3' 'AU/ZaZampler.component' 'Standalone/ZaZampler.app'; do
  bundle="$products/$relative"
  [[ -d "$bundle" ]] || { echo "Missing: $bundle" >&2; exit 1; }
  codesign --verify --deep --strict --verbose=2 "$bundle"
  executable=$(/usr/libexec/PlistBuddy -c 'Print :CFBundleExecutable' "$bundle/Contents/Info.plist")
  lipo "$bundle/Contents/MacOS/$executable" -verify_arch arm64
  ditto "$bundle" "$package/$relative"
done
ditto Demo "$package/Demo"
ditto docs "$package/docs"
cp LICENSE README.md "$package/"
cp scripts/install-macos.sh scripts/validate-macos.sh "$package/scripts/"
cat > "$package/INSTALL.txt" <<'TEXT'
ZaZampler macOS arm64 — development build (ad-hoc signed, not notarized)

Extract this entire folder. Quit your DAW, open Terminal in this folder, and run:
  bash scripts/install-macos.sh
Then close the standalone app, restart your DAW and rescan plugins.
Existing installations are backed up by the installer.

VST3/ contains the VST3 plugin, AU/ the AUv2 component, and Standalone/
contains the application with its embedded AUv3 extension. Do not move the
extension out of the app. Demo/ contains a test SFZ instrument and audio.

This build is not Developer ID signed or notarized. macOS may block launching
or loading downloaded bundles. See docs/GITHUB_ACTIONS.md for limitations.
Actual DAW playback and AUv3 registration still need testing on your Mac.
TEXT
{
  printf 'Commit: %s\n' "$(git rev-parse HEAD)"
  printf 'Built (UTC): %s\n' "$(date -u +%FT%TZ)"
  sw_vers
  xcodebuild -version
} > "$package/BUILD-INFO.txt"
archive="$PWD/dist/ZaZampler-macOS-arm64.zip"
rm -f "$archive"
# Pre-zip with ditto: uploading bundle directories directly loses Unix permissions.
ditto -c -k --sequesterRsrc --keepParent "$package" "$archive"
echo "Packaged: $archive"
