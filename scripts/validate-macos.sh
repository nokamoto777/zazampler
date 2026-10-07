#!/bin/bash
set -euo pipefail
app="$HOME/Applications/ZaZampler.app"
file "$app/Contents/MacOS/ZaZampler"
codesign --verify --deep --strict --verbose=2 "$app"
# AUv2 and AUv3 share the component identifiers: this checks whichever macOS resolves.
auval -v aumu Qsfz Qsmp
pluginkit -m -A -D | grep -i zazampler || {
  echo 'AUv3 not listed. Run the containing app once and verify its embedded extension.' >&2; exit 1;
}
echo 'Also test AUv3 and VST3 separately in your actual DAWs (see docs/MAC_ACCEPTANCE.md).'
