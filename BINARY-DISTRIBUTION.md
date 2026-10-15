# mp_audio_stream-example binary distribution

The application is built from `example/`. Tags and artifact versions follow the plugin version in the repository root, rather than the placeholder example version.

## Verify a CI build, then release it

The **Build binaries** workflow runs tests and a WebAssembly/JavaScript build for pull requests and pushes to `develop`. A push to `main`, normally caused by merging a PR, and a manual run build every native platform using a fixed matrix. Add the `ci:full` label to a PR when native builds are needed before merging; remove it after verification to keep subsequent pushes lightweight. Pull requests never use signing secrets. Download the `release-*` artifacts from the run page and verify the applications internally. The artifact ZIP is a container; extract it to get the APK, IPA or platform archive.

After every job is green and the binaries have passed internal checks, tag that exact commit. The tag must match the repository root's `pubspec.yaml` version, without its `+build` suffix. Commit any version change before building and verifying it.

```sh
git tag v1.0.0 <verified-commit-sha>
git push origin v1.0.0
```

A successful full main/manual build uploads a `full-build-success` completion artifact after tests, Web and every native build have succeeded. **Release verified binaries** uses `dawidd6/action-download-artifact` to find that artifact for the exact tagged commit, validates the source run and the complete platform set, then downloads the immutable binaries with the official `actions/download-artifact` action. A digest mismatch fails the download. PR builds cannot be released: verify a full main/manual build of the commit being tagged. Artifacts are retained for 90 days; after expiry, build and verify again before releasing.

To select a specific verified run or retry a draft release, run **Release verified binaries** manually with the existing `tag` and `build_run_id`:

```sh
gh workflow run release.yaml -f tag=v1.0.0 -f build_run_id=<verified-run-id>
```

`softprops/action-gh-release` attaches the existing binaries to a draft, and the workflow publishes only after all uploads succeed. No rebuilding or re-signing occurs during release. Published releases are left untouched. Prerelease versions such as `1.0.1-rc.1` use the matching `v1.0.1-rc.1` tag. A new commit requires another full build and internal verification.

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

Release files include `SHA256SUMS.txt` and `BUILD-METADATA.json`, which identifies the source commit, CI run and immutable CI artifact IDs and digests. GitHub-hosted runners use Flutter 3.47.5. Windows ARM64 uses a native Dart SDK on the ARM64 runner; macOS archives are checked for both architectures.

## Optional signing secrets

Without signing secrets, Android uses the existing debug-key signing configuration for a release-mode build, labeled `test-signed`. CI runners generate temporary keys, so these APKs are for internal checks and cannot reliably update an existing installation. Configure a persistent release key before verifying binaries intended for ongoing distribution. `r0adkll/sign-android-release` signs the APKs and AAB during CI, using the persistent key or the temporary debug key. Its Node 24 revision is pinned because the older public `v1` tag uses Node 12. The exact signed CI binaries are then promoted to Releases.

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

`apple-actions/import-codesign-certs` handles the temporary Keychain, and `timheuer/base64-to-file` decodes the profile and export options. The iOS bundle identifier is taken from the profile; the source project is not rewritten. Use an ad-hoc profile with registered devices for direct internal device installation, or an App Store profile for a store-ready export. Store uploads, TestFlight and notarization are separate operations. Partial signing configuration fails the build. Signing material is written to temporary runner files and build directories, excluded from artifacts and removed after building. Secrets are not used for fork pull-request builds.

## GitHub Pages

Successful `main` builds publish the verified Web archive at https://reki2000.github.io/mp-audio-stream/. Successful `develop` builds publish at https://reki2000.github.io/mp-audio-stream/develop/. PR builds upload artifacts and run the Pages action in dry-run mode; manual builds only upload artifacts. Neither publishes the site. `JamesIves/github-pages-deploy-action` publishes to `gh-pages`, uses a target folder for develop, and protects develop during root cleanup. Main and develop deployments share a serialized publishing job; each replaces its own files and retains the other site. The same portable Web archive can be served at either path without rebuilding or changing it during deployment.

For a repository's first deployment, select **Settings > Pages > Build and deployment > Deploy from a branch**, then choose **gh-pages / (root)** once the first successful branch build creates `gh-pages`. The branch contains static output and `.nojekyll`, not application source. Existing branch-based Pages settings can be retained. All publication is performed by GitHub Actions; Codemagic is not needed.

The two paths share the `reki2000.github.io` origin. Browser storage such as localStorage and IndexedDB is not isolated by URL path. Use different storage names or origins if development data must be isolated from stable data. These applications use hash-based navigation; a portable relative base assumes the site is opened at its directory URL with a trailing slash.

## Workflow maintenance

Workflow YAML defines the triggers, job dependencies, runners and application tests. Shared build, packaging, signing preparation, release verification and Pages commands live in [mp-build-tools](https://github.com/reki2000/mp-build-tools).

Each shared Action is pinned to the same reviewed commit SHA. Update the pins together after the shared Action checks and this repository's full native/Web CI pass. No build or release helper scripts are duplicated in this repository.
