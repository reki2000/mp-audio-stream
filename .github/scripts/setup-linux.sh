#!/usr/bin/env bash
set -euo pipefail

sudo apt-get update
sudo apt-get install -y clang cmake ninja-build pkg-config libgtk-3-dev libasound2-dev libstdc++-12-dev

# Flutter has no archived Linux ARM64 SDK; bootstrap the pinned source instead.
if [[ "$ARCH" == arm64 ]]; then
  sdk="$RUNNER_TEMP/flutter-arm64"
  if [[ ! -d "$sdk/.git" ]]; then
    git clone --depth 1 --branch "$FLUTTER_VERSION" https://github.com/flutter/flutter.git "$sdk"
  fi
  echo "$sdk/bin" >> "$GITHUB_PATH"
  "$sdk/bin/flutter" --version
fi
