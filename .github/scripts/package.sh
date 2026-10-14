#!/usr/bin/env bash
# Package built applications; workflow actions handle signing and publication.
set -euo pipefail

target=$1 arch=$2
build="$PWD/$APP_DIR/build"
output="$PWD/release-assets"
name="$ARTIFACT_NAME-$VERSION-$target-$arch"
mkdir -p "$output"

case "$target" in
  android)
    signing=test-signed
    [[ "$ANDROID_SIGNING" != true ]] || signing=signed
    for abi in armeabi-v7a arm64-v8a x86_64; do
      cp "$build/app/outputs/flutter-apk/app-$abi-release-signed.apk" "$output/$name-$abi-$signing.apk"
    done
    cp "$build/app/outputs/bundle/release/app-release.aab" "$output/$name-$signing.aab"
    ;;
  linux)
    tar -czf "$output/$name.tar.gz" -C "$build/linux/$arch/release/bundle" .
    ;;
  windows)
    (cd "$build/windows/$arch/runner/Release" && 7z a -tzip "$output/$name.zip" .)
    ;;
  macos)
    apps=("$build/macos/Build/Products/Release/"*.app)
    [[ ${#apps[@]} == 1 && -d "${apps[0]}" ]]
    app="${apps[0]}"
    executable=$(/usr/libexec/PlistBuddy -c 'Print :CFBundleExecutable' "$app/Contents/Info.plist")
    for binary in "MacOS/$executable" Frameworks/App.framework/App Frameworks/FlutterMacOS.framework/FlutterMacOS; do
      lipo "$app/Contents/$binary" -verify_arch x86_64 arm64
    done
    args=(--force --deep --sign -)
    entitlements="$PWD/$APP_DIR/macos/Runner/Release.entitlements"
    [[ ! -f "$entitlements" ]] || args+=(--entitlements "$entitlements")
    codesign "${args[@]}" "$app"
    codesign --verify --deep --strict "$app"
    ditto -c -k --sequesterRsrc --keepParent "$app" "$output/$name-adhoc.zip"
    ;;
  ios)
    if [[ "$IOS_SIGNING" == true ]]; then
      ipas=("$build/ios/ipa/"*.ipa)
      [[ ${#ipas[@]} == 1 ]]
      cp "${ipas[0]}" "$output/$name-signed.ipa"
    else
      payload="$RUNNER_TEMP/unsigned-ios/Payload"
      mkdir -p "$payload"
      apps=("$build/ios/iphoneos/"*.app)
      [[ ${#apps[@]} == 1 ]]
      ditto "${apps[0]}" "$payload/$(basename "${apps[0]}")"
      ditto -c -k --keepParent "$payload" "$output/$name-unsigned.ipa"
    fi
    ;;
  ios-simulator)
    apps=("$build/ios/iphonesimulator/"*.app)
    [[ ${#apps[@]} == 1 ]]
    ditto -c -k --keepParent "${apps[0]}" "$output/$name-debug.zip"
    ;;
  *) echo "Unsupported package target: $target" >&2; exit 1 ;;
esac
