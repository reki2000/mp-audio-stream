#!/usr/bin/env bash
# Run from the repository root with the workflow's application and version environment.
set -euo pipefail

target=$1
cd "$APP_DIR"
flutter pub get
args=(--build-name="${BUILD_NAME:-$VERSION}" --build-number="$GITHUB_RUN_NUMBER" --dart-define=APP_VERSION="$VERSION")

case "$target" in
  web)
    flutter build web --wasm --release --base-href=/ --no-web-resources-cdn "${args[@]}"
    ;;
  android)
    flutter build apk --release --split-per-abi "${args[@]}"
    flutter build appbundle --release "${args[@]}"
    ;;
  ios)
    if [[ "$IOS_SIGNING" == true ]]; then
      flutter build ipa --release --export-options-plist="$IOS_EXPORT_OPTIONS_PATH" "${args[@]}"
    else
      flutter build ios --release --no-codesign "${args[@]}"
    fi
    ;;
  ios-simulator)
    flutter build ios --simulator --debug --no-codesign "${args[@]}"
    ;;
  macos)
    flutter config --no-enable-macos-arm64-only
    FLUTTER_XCODE_CODE_SIGNING_ALLOWED=NO flutter build macos --release "${args[@]}"
    ;;
  linux|windows)
    flutter build "$target" --release "${args[@]}"
    ;;
  *) echo "Unsupported build target: $target" >&2; exit 1 ;;
esac
