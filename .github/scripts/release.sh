#!/usr/bin/env bash
set -euo pipefail

read_version() {
  version=$(awk '$1 == "version:" {gsub(/["\047]/, "", $2); sub(/\+.*/, "", $2); print $2; exit}' pubspec.yaml)
  [[ "$version" =~ ^[0-9]+\.[0-9]+\.[0-9]+(-[0-9A-Za-z.-]+)?$ ]]
}

case "$1" in
  version)
    read_version
    printf 'version=%s\nbuild_name=%s\n' "$version" "${version%%-*}" >> "$GITHUB_OUTPUT"
    ;;
  check-tag)
    read_version
    [[ "$RELEASE_TAG" == "v$version" ]]
    printf 'commit=%s\nversion=%s\n' "$(git rev-parse HEAD)" "$version" >> "$GITHUB_OUTPUT"
    if [[ "$(gh release view "$RELEASE_TAG" --json isDraft --jq .isDraft 2>/dev/null || true)" == false ]]; then
      echo 'publish=false' >> "$GITHUB_OUTPUT"
    else
      echo 'publish=true' >> "$GITHUB_OUTPUT"
    fi
    ;;
  checksums)
    for directory in release-assets/release-*; do
      for file in "$directory"/*; do
        [[ -s "$file" && "$(basename "$file")" == "$ARTIFACT_NAME-$VERSION-"* ]]
      done
      (cd "$directory" && sha256sum *)
    done > SHA256SUMS.txt
    ;;
  *) echo "Unsupported release command: $1" >&2; exit 1 ;;
esac
