#!/usr/bin/env bash
# Usage: bash scripts/build-fx-probe-linux.sh /path/to/JUCE-8.0.6 /path/to/build
set -euo pipefail
repo="$(cd "$(dirname "$0")/.." && pwd)"
juce="$(cd "$1" && pwd)"
mkdir -p "$2"
out="$(cd "$2" && pwd)"
cxx="${CXX:-c++}"
flags=(-std=c++17 -O2 -pthread -DJUCE_GLOBAL_MODULE_SETTINGS_INCLUDED=1
       -DJUCE_STANDALONE_APPLICATION=1 -DJUCE_USE_CURL=0 -DJUCE_WEB_BROWSER=0
       -I"$juce/modules" -I"$repo/Source" -I"$repo/Tests")
sources=("$repo/Tests/FxProbe.cpp" "$repo/Source/Effects.cpp"
         "$juce/modules/juce_audio_basics/juce_audio_basics.cpp"
         "$juce/modules/juce_core/juce_core.cpp"
         "$juce/modules/juce_core/juce_core_CompilationTime.cpp")
objects=()
for src in "${sources[@]}"; do
    obj="$out/$(basename "$src").o"
    "$cxx" "${flags[@]}" -c "$src" -o "$obj"
    objects+=("$obj")
done
"$cxx" -pthread "${objects[@]}" -ldl -lrt -lz -o "$out/fx_probe"
"$out/fx_probe" --self-test
