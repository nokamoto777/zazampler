#!/bin/bash
set -euo pipefail
cd "$(dirname "$0")/.."
[[ "$(uname -s)" == Darwin ]] || { echo 'macOS required' >&2; exit 1; }
products="$PWD/build-macos/ZaZampler_artefacts/Release"
for path in 'VST3/ZaZampler.vst3' 'AU/ZaZampler.component' 'Standalone/ZaZampler.app'; do
  [[ -d "$products/$path" ]] || { echo "Missing build output: $path" >&2; exit 1; }
done
# Preserve previous builds instead of deleting them; quit hosts before running.
stamp="$(date +%Y%m%d-%H%M%S)"
backup="$HOME/Library/Application Support/ZaZampler/Backups/$stamp"
copy_bundle() {
  src="$1"; dest="$2"
  mkdir -p "$(dirname "$dest")"
  if [[ -e "$dest" ]]; then mkdir -p "$backup"; mv "$dest" "$backup/$(basename "$dest")"; fi
  ditto "$src" "$dest"
}
# Retire the old product name into the same reversible backup directory.
for legacy in "$HOME/Library/Audio/Plug-Ins/VST3/QSampler.vst3" "$HOME/Library/Audio/Plug-Ins/Components/QSampler.component" "$HOME/Applications/QSampler.app"; do
  if [[ -e "$legacy" ]]; then
    mkdir -p "$backup"
    mv "$legacy" "$backup/$(basename "$legacy")"
  fi
done
copy_bundle "$products/VST3/ZaZampler.vst3" "$HOME/Library/Audio/Plug-Ins/VST3/ZaZampler.vst3"
copy_bundle "$products/AU/ZaZampler.component" "$HOME/Library/Audio/Plug-Ins/Components/ZaZampler.component"
copy_bundle "$products/Standalone/ZaZampler.app" "$HOME/Applications/ZaZampler.app"
# LaunchServices discovers the embedded AUv3 when its containing application runs.
open "$HOME/Applications/ZaZampler.app"
echo 'Installed. Close the standalone, restart your DAW, and rescan plugins.'
echo 'AUv3 appears only in hosts supporting AUv3; use VST3 or AU in other hosts.'
