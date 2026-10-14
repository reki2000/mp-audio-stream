#!/usr/bin/env bash
# Public actions perform signing; these commands prepare and remove runner files.
set -euo pipefail

case "$1" in
  validate)
    if [[ "$ANY_ANDROID" != "$ALL_ANDROID" || "$ANY_IOS" != "$ALL_IOS" ]]; then
      echo 'Configure all four signing secrets for each enabled platform.' >&2
      exit 1
    fi
    ;;
  configure-ios)
    security cms -D -i "$RUNNER_TEMP/signing.mobileprovision" > "$RUNNER_TEMP/profile.plist"
    uuid=$(/usr/libexec/PlistBuddy -c 'Print :UUID' "$RUNNER_TEMP/profile.plist")
    team=$(/usr/libexec/PlistBuddy -c 'Print :TeamIdentifier:0' "$RUNNER_TEMP/profile.plist")
    identifier=$(/usr/libexec/PlistBuddy -c 'Print :Entitlements:application-identifier' "$RUNNER_TEMP/profile.plist")
    [[ "$identifier" != *'*'* ]]
    profiles="$HOME/Library/MobileDevice/Provisioning Profiles"
    mkdir -p "$profiles"
    cp "$RUNNER_TEMP/signing.mobileprovision" "$profiles/$uuid.mobileprovision"
    printf '%s\n' "IOS_EXPORT_OPTIONS_PATH=$RUNNER_TEMP/ExportOptions.plist" \
      "FLUTTER_XCODE_DEVELOPMENT_TEAM=$team" "FLUTTER_XCODE_PRODUCT_BUNDLE_IDENTIFIER=${identifier#*.}" \
      'FLUTTER_XCODE_CODE_SIGN_STYLE=Manual' "FLUTTER_XCODE_PROVISIONING_PROFILE_SPECIFIER=$uuid" \
      'FLUTTER_XCODE_CODE_SIGN_IDENTITY=Apple Distribution' >> "$GITHUB_ENV"
    ;;
  prepare-android)
    echo "BUILD_TOOLS_VERSION=$(ls "$ANDROID_HOME/build-tools" | sort -V | tail -1)" >> "$GITHUB_ENV"
    if [[ "$ANDROID_SIGNING" != true ]]; then
      keytool -genkeypair -keystore "$RUNNER_TEMP/test-signing.jks" -storepass android -keypass android -alias androiddebugkey -keyalg RSA -keysize 2048 -validity 30 -dname 'CN=CI test key'
      key=$(base64 -w0 "$RUNNER_TEMP/test-signing.jks")
      echo "::add-mask::$key"
      echo "key=$key" >> "$GITHUB_OUTPUT"
    fi
    zip -d "$APP_DIR/build/app/outputs/bundle/release/app-release.aab" 'META-INF/*.SF' 'META-INF/*.RSA' 'META-INF/*.DSA' 'META-INF/*.EC' 'META-INF/MANIFEST.MF' || [[ $? == 12 ]]
    ;;
  cleanup)
    rm -f "$APP_DIR/build/app/outputs/flutter-apk/signingKey.jks" "$APP_DIR/build/app/outputs/bundle/release/signingKey.jks"
    rm -f "$RUNNER_TEMP/test-signing.jks" "$RUNNER_TEMP/signing.mobileprovision" "$RUNNER_TEMP/ExportOptions.plist" "$RUNNER_TEMP/profile.plist"
    if [[ -n "${FLUTTER_XCODE_PROVISIONING_PROFILE_SPECIFIER:-}" ]]; then
      rm -f "$HOME/Library/MobileDevice/Provisioning Profiles/$FLUTTER_XCODE_PROVISIONING_PROFILE_SPECIFIER.mobileprovision"
    fi
    ;;
  *) echo "Unsupported signing command: $1" >&2; exit 1 ;;
esac
