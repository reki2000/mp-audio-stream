#!/usr/bin/env bash
set -euo pipefail

args=()
case "${1:-vm}" in
  vm) ;;
  js) args=(--platform chrome) ;;
  wasm) args=(--platform chrome --wasm) ;;
  *) echo 'Expected vm, js or wasm' >&2; exit 1 ;;
esac

flutter pub get
(cd example && flutter pub get && flutter test "${args[@]}")
