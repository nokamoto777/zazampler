#!/bin/bash
set -euo pipefail
cd "$(dirname "$0")/.."
if [[ "$(uname -s)" != Darwin ]]; then echo 'Run this script on macOS with full Xcode installed.' >&2; exit 1; fi
if ! xcrun --find xcodebuild >/dev/null 2>&1 || ! xcodebuild -version >/dev/null 2>&1; then
  echo 'Install full Xcode, launch it once, and select it in Xcode > Settings > Locations > Command Line Tools.' >&2; exit 1
fi
if ! command -v cmake >/dev/null; then echo 'CMake is required: brew install cmake (or install from cmake.org).' >&2; exit 1; fi
args=(-G Xcode -DCMAKE_OSX_ARCHITECTURES=arm64 -DCMAKE_OSX_DEPLOYMENT_TARGET=12.0)
if [[ -n "${DEVELOPMENT_TEAM:-}" ]]; then
  args+=("-DCMAKE_XCODE_ATTRIBUTE_DEVELOPMENT_TEAM=$DEVELOPMENT_TEAM" -DCMAKE_XCODE_ATTRIBUTE_CODE_SIGN_STYLE=Automatic)
else
  args+=('-DCMAKE_XCODE_ATTRIBUTE_CODE_SIGN_IDENTITY=-' -DCMAKE_XCODE_ATTRIBUTE_CODE_SIGN_STYLE=Manual)
fi
cmake -S . -B build-macos "${args[@]}"
cmake --build build-macos --config Release --target ZaZampler_VST3 ZaZampler_AU ZaZampler_Standalone engine_test bank_test processor_test --parallel "${BUILD_JOBS:-4}"
ctest --test-dir build-macos -C Release --output-on-failure
app="$PWD/build-macos/ZaZampler_artefacts/Release/Standalone/ZaZampler.app"
if ! find "$app/Contents/PlugIns" -type d -name '*.appex' -print -quit | grep -q .; then
  echo 'ERROR: AUv3 extension was not embedded in the containing app.' >&2; exit 1
fi
codesign --verify --deep --strict --verbose=2 "$app"
echo 'Build complete. Install with: bash scripts/install-macos.sh'
