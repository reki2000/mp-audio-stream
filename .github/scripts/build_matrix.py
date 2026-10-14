"""Select full builds for main/manual runs and affected native platforms for PRs."""

import json
import os
from pathlib import Path
import subprocess


BUILDS = [
    {"target": "android", "arch": "all", "runner": "ubuntu-22.04", "sdk_arch": "x64"},
    {"target": "linux", "arch": "x64", "runner": "ubuntu-22.04", "sdk_arch": "x64"},
    {"target": "linux", "arch": "arm64", "runner": "ubuntu-22.04-arm", "sdk_arch": "arm64"},
    {"target": "windows", "arch": "x64", "runner": "windows-2022", "sdk_arch": "x64"},
    {"target": "windows", "arch": "arm64", "runner": "windows-11-arm", "sdk_arch": "x64"},
    {"target": "macos", "arch": "universal", "runner": "macos-15", "sdk_arch": "arm64"},
    {"target": "ios", "arch": "arm64", "runner": "macos-15", "sdk_arch": "arm64"},
    {"target": "ios-simulator", "arch": "arm64", "runner": "macos-15", "sdk_arch": "arm64"},
    {"target": "web", "arch": "browser", "runner": "ubuntu-22.04", "sdk_arch": "x64"},
]


def select_matrix(event, ref, changed=()):
    if event == "workflow_dispatch" or ref == "refs/heads/main":
        return BUILDS
    targets = {"web"}
    if event == "pull_request":
        for filename in changed:
            parts = Path(filename).parts
            if parts and parts[0] == "example":
                parts = parts[1:]
            if not parts:
                continue
            if parts[0] in ("src", "native"):
                return BUILDS
            if parts[0] in ("android", "linux", "windows", "macos", "ios"):
                targets.add(parts[0])
                if parts[0] == "ios":
                    targets.add("ios-simulator")
    return [build for build in BUILDS if build["target"] in targets]


if __name__ == "__main__":
    event = os.environ["GITHUB_EVENT_NAME"]
    changed = []
    if event == "pull_request":
        base = json.loads(Path(os.environ["GITHUB_EVENT_PATH"]).read_text())["pull_request"]["base"]["sha"]
        changed = subprocess.check_output(["git", "diff", "--name-only", base, "HEAD"], text=True).splitlines()
    matrix = {"include": select_matrix(event, os.environ["GITHUB_REF"], changed)}
    with open(os.environ["GITHUB_OUTPUT"], "a") as output:
        output.write("matrix=" + json.dumps(matrix, separators=(",", ":")) + "\n")
    print("Building: " + ", ".join(f"{build['target']}/{build['arch']}" for build in matrix["include"]))
