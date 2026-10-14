# mp_audio_stream-example binary distribution

The application is built from `example/`. Tags and artifact versions follow the plugin version in the repository root, rather than the placeholder example version.

## Verify a CI build, then release it

The **Build binaries** workflow runs on branch pushes and can also be started from the Actions tab. Fork pull requests are built without signing secrets. Each run tests the repository and builds all supported platforms. Download its `release-*` artifacts from the run page and verify the applications internally. The artifact ZIP is a container; extract it to get the APK, IPA or platform archive.

After every job is green and the binaries have passed internal checks, tag that exact commit. The tag must match the repository root's `pubspec.yaml` version, without its `+build` suffix. Commit any version change before building and verifying it.

```sh
git tag v1.0.0 <verified-commit-sha>
git push origin v1.0.0
```

**Release verified binaries** finds the latest successful **Build binaries** run for the exact tagged commit, downloads its artifacts, checks every platform manifest and SHA-256 digest, and publishes them without rebuilding. A pull-request merge build is not used: verify a branch-push build of the commit being tagged. Tests and every platform build must have succeeded. Missing, expired, incomplete, mismatched or modified artifacts prevent publication. Artifacts are retained for 90 days; after expiry, build and verify again before releasing.

To choose a specific verified run or retry a draft release, run **Release verified binaries** manually with the existing `tag` and `build_run_id`. With GitHub CLI:

```sh
gh workflow run release.yaml -f tag=v1.0.0 -f build_run_id=<verified-run-id>
```

The release is assembled as a draft and published only after all assets have uploaded. Existing draft release notes are retained. Published releases are not overwritten. Prerelease versions such as `1.0.1-rc.1` use the matching `v1.0.1-rc.1` tag and are published as prereleases. A new branch commit requires another build and internal verification.

## Files and installation

| Platform | Files | Use |
|---|---|---|
| Android | ARM32, ARM64 and x86-64 release APKs; AAB | Install the APK for the device architecture. The AAB is for store upload, not direct installation. |
| iOS device | ARM64 IPA | An `unsigned` IPA needs signing before device installation. A signed IPA remains limited by its provisioning profile and export method. |
| iOS simulator | ARM64 debug app ZIP | Extract and install on an Apple Silicon simulator with `xcrun simctl install booted <app-path>`. This is not a device IPA. |
| Windows | x64 and ARM64 ZIPs | Extract the entire directory and run the EXE. Flutter assets, plugin libraries and the matching Visual C++ runtime DLLs are included. |
| macOS | Universal Intel/Apple Silicon app ZIP | Extract and open the app. It is ad-hoc signed with its release entitlements, not Developer ID signed or notarized; macOS may require approval in Privacy & Security. |
| Linux | x64 and ARM64 tar.gz bundles | Extract the entire directory and run the executable. Requires GTK 3 and the distribution's audio libraries. Built on Ubuntu 22.04. |
| Web | WebAssembly and JavaScript fallback ZIP | Extract and serve over HTTP(S), for example with `python3 -m http.server`. Opening `index.html` as a local file is not supported. File-access features may require HTTPS or localhost. |

Release files include `SHA256SUMS.txt` and `BUILD-METADATA.json`, which identifies the source commit, CI run and per-platform binary digests. GitHub-hosted runners use Flutter 3.47.5. Windows ARM64 uses a native Dart SDK on the ARM64 runner; macOS archives are checked for both architectures.

## Optional signing secrets

Without signing secrets, Android uses the existing debug-key signing configuration for a release-mode build, labeled `test-signed`. CI runners generate temporary keys, so these APKs are for internal checks and cannot reliably update an existing installation. Configure a persistent release key before verifying binaries intended for ongoing distribution. The exact signed CI binaries are then promoted to Releases.

Set all four Android repository secrets together:

| Secret | Value |
|---|---|
| `ANDROID_KEYSTORE_BASE64` | Base64-encoded JKS or PKCS12 keystore |
| `ANDROID_KEYSTORE_PASSWORD` | Keystore password |
| `ANDROID_KEY_ALIAS` | Signing key alias |
| `ANDROID_KEY_PASSWORD` | Signing key password |

For signed iOS builds, set all four secrets together:

| Secret | Value |
|---|---|
| `IOS_CERTIFICATE_BASE64` | Base64-encoded Apple Distribution certificate with its private key, exported as P12 |
| `IOS_CERTIFICATE_PASSWORD` | P12 password |
| `IOS_PROFILE_BASE64` | Base64-encoded explicit application provisioning profile |
| `IOS_EXPORT_OPTIONS_BASE64` | Base64-encoded Xcode ExportOptions.plist matching the certificate, team, app and distribution method |

The iOS bundle identifier is taken from the profile; the source project is not rewritten. Use an ad-hoc profile with registered devices for direct internal device installation, or an App Store profile for a store-ready export. Store uploads, TestFlight and notarization are separate operations. Partial signing configuration fails the build. Signing material is written to runner temporary storage, excluded from artifacts and removed after building. Secrets are not used for fork pull-request builds.
