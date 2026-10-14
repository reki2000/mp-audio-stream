"""Promote an exact successful CI build to a version-tagged GitHub Release."""

import hashlib
import json
import os
from pathlib import Path
import subprocess

from release import metadata


TARGETS = {("android", "all"), ("linux", "x64"), ("linux", "arm64"),
           ("windows", "x64"), ("windows", "arm64"), ("macos", "universal"),
           ("ios", "arm64"), ("ios-simulator", "arm64"), ("web", "browser")}


def gh(*args):
    return subprocess.check_output(["gh", *args], text=True)


def select_run(runs, commit, requested=""):
    for run in runs:
        if requested and str(run["id"]) != requested:
            continue
        if (run["head_sha"] == commit and run["status"] == "completed"
                and run["conclusion"] == "success"
                and (run["event"] == "workflow_dispatch" or (run["event"] == "push" and run.get("head_branch") == "main"))):
            return run
    raise ValueError("No successful full Build binaries run for this exact commit (main push or manual run). Test a successful CI build before tagging it.")


def verify_assets(directory, commit, run_id, version):
    manifests = [json.loads(p.read_text()) for p in directory.rglob("manifest-*.json")]
    seen = set()
    assets = {}
    for manifest in manifests:
        target = (manifest["target"], manifest["architecture"])
        if target in seen or target not in TARGETS:
            raise ValueError(f"Unexpected or duplicate build target: {target}")
        seen.add(target)
        if (manifest["commit"] != commit or manifest["run_id"] != str(run_id)
                or manifest["version"] != version):
            raise ValueError("CI artifacts do not match the tagged commit, build run and version")
        if not manifest["assets"]:
            raise ValueError(f"Empty build manifest for {target}")
        for name, digest in manifest["assets"].items():
            if Path(name).name != name or name in assets:
                raise ValueError(f"Unsafe or duplicate asset name: {name}")
            matches = list(directory.rglob(name))
            if len(matches) != 1 or not matches[0].is_file():
                raise ValueError(f"Missing or ambiguous asset: {name}")
            with matches[0].open("rb") as stream:
                actual = hashlib.file_digest(stream, "sha256").hexdigest()
            if actual != digest:
                raise ValueError(f"Asset checksum mismatch: {name}")
            assets[name] = (matches[0], digest)
    if seen != TARGETS:
        raise ValueError(f"Missing build targets: {sorted(TARGETS - seen)}")
    return manifests, assets


def main():
    root = Path(__file__).resolve().parents[2]
    repository = os.environ["GITHUB_REPOSITORY"]
    tag = os.environ["RELEASE_TAG"]
    if not tag.startswith("v"):
        raise ValueError("Use a version tag starting with v")
    commit = subprocess.check_output(["git", "rev-parse", "HEAD"], text=True).strip()
    remote = json.loads(gh("api", f"repos/{repository}/commits/{tag}"))["sha"]
    if commit != remote:
        raise ValueError("Checkout does not match the release tag")
    os.environ["GITHUB_REF"] = f"refs/tags/{tag}"
    version = metadata(root)["version"]
    requested = os.environ.get("BUILD_RUN_ID", "")
    if requested:
        # Inspect the requested run directly so that old verified builds can still be promoted.
        runs = [json.loads(gh("api", f"repos/{repository}/actions/runs/{requested}"))]
        workflow = json.loads(gh("api", f"repos/{repository}/actions/workflows/build-binaries.yaml"))
        if runs[0]["workflow_id"] != workflow["id"]:
            raise ValueError("The selected run is not the Build binaries workflow")
    else:
        runs = json.loads(gh("api", f"repos/{repository}/actions/workflows/build-binaries.yaml/runs?head_sha={commit}&status=success&per_page=100"))["workflow_runs"]
    run = select_run(runs, commit, requested)
    directory = root / "ci-artifacts"
    directory.mkdir()
    gh("run", "download", str(run["id"]), "--repo", repository, "--pattern", "release-*", "--dir", str(directory))
    manifests, assets = verify_assets(directory, commit, run["id"], version)
    sums = directory / "SHA256SUMS.txt"
    sums.write_text("".join(f"{digest}  {name}\n" for name, (_, digest) in sorted(assets.items())))
    provenance = directory / "BUILD-METADATA.json"
    provenance.write_text(json.dumps({"tag": tag, "commit": commit, "ci_run": run["html_url"],
                                     "builds": manifests}, indent=2) + "\n")
    result = subprocess.run(["gh", "release", "view", tag, "--repo", repository, "--json", "isDraft"],
                            text=True, capture_output=True)
    if result.returncode == 0:
        if not json.loads(result.stdout)["isDraft"]:
            print(f"Release {tag} is already published; existing assets are retained.")
            return
    else:
        notes = directory / "notes.md"
        notes.write_text(f"Binaries promoted without rebuilding from [CI run {run['id']}]({run['html_url']}) for commit `{commit}`.\n\n"
                         f"See [BINARY-DISTRIBUTION.md](https://github.com/{repository}/blob/{tag}/BINARY-DISTRIBUTION.md) for installation and signing details. "
                         "Test-signed Android builds use a temporary key; unsigned iOS IPAs need signing before installation. "
                         "The macOS app is ad-hoc signed and is not notarized. Web archives must be served over HTTP(S).\n")
        command = ["release", "create", tag, "--repo", repository, "--verify-tag", "--draft", "--generate-notes", "--notes-file", str(notes)]
        if "-" in version:
            command.append("--prerelease")
        gh(*command)
    gh("release", "upload", tag, *[str(path) for path, _ in assets.values()], str(sums), str(provenance),
       "--repo", repository, "--clobber")
    gh("release", "edit", tag, "--repo", repository, "--draft=false", "--verify-tag")
    print(f"Published {tag} from CI run {run['id']}: {len(assets)} binaries.")


if __name__ == "__main__":
    main()
