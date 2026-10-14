"""Publish verified Web artifacts to stable and develop paths on gh-pages."""

import json
import os
from pathlib import Path
import shutil
import subprocess
import tempfile
import zipfile

from release import digest_file


def replace_site(tree, site, channel):
    if channel == "develop":
        destination = tree / "develop"
        if destination.exists():
            shutil.rmtree(destination)
        shutil.copytree(site, destination)
    elif channel == "main":
        # Keep the development site and GitHub Pages metadata when replacing stable.
        for path in tree.iterdir():
            if path.name not in (".git", "develop", "CNAME", ".nojekyll"):
                if path.is_dir():
                    shutil.rmtree(path)
                else:
                    path.unlink()
        shutil.copytree(site, tree, dirs_exist_ok=True)
    else:
        raise ValueError(f"Unsupported Pages channel: {channel}")
    (tree / ".nojekyll").touch()


def extract_site(archive, destination):
    with zipfile.ZipFile(archive) as source:
        for entry in source.infolist():
            path = Path(entry.filename)
            if path.is_absolute() or ".." in path.parts or entry.filename.startswith("\\"):
                raise ValueError("Unsafe path in Web artifact")
        source.extractall(destination)
    if not (destination / "index.html").is_file():
        raise ValueError("Web artifact has no index.html")


def main():
    root = Path(__file__).resolve().parents[2]
    artifacts = root / "release-assets"
    manifest = json.loads((artifacts / "manifest-web-browser.json").read_text())
    if manifest["commit"] != os.environ["GITHUB_SHA"] or manifest["run_id"] != os.environ["GITHUB_RUN_ID"]:
        raise ValueError("Pages artifacts do not match this CI commit and run")
    if len(manifest["assets"]) != 1:
        raise ValueError("Expected exactly one Web archive")
    name, digest = next(iter(manifest["assets"].items()))
    if Path(name).name != name:
        raise ValueError("Unsafe Web archive name")
    archive = artifacts / name
    if digest_file(archive) != digest:
        raise ValueError("Web archive checksum mismatch")
    channel = os.environ["GITHUB_REF_NAME"]
    tree = root / ".worktrees/pages"
    tree.parent.mkdir(exist_ok=True)
    def git(*args, cwd=root):
        return subprocess.run(["git", *args], cwd=cwd, check=True)
    probe = subprocess.run(["git", "ls-remote", "--exit-code", "--heads", "origin", "gh-pages"],
                           cwd=root, capture_output=True)
    if probe.returncode == 0:
        git("fetch", "--depth=1", "origin", "gh-pages")
        git("worktree", "add", "--detach", str(tree), "FETCH_HEAD")
    elif probe.returncode == 2:
        git("worktree", "add", "--detach", str(tree), "HEAD")
        git("checkout", "--orphan", "gh-pages", cwd=tree)
        for path in tree.iterdir():
            if path.name == ".git":
                continue
            if path.is_dir():
                shutil.rmtree(path)
            else:
                path.unlink()
        # Clear the orphan branch's index so source files are not committed as site assets.
        git("rm", "-r", "--cached", "--ignore-unmatch", ".", cwd=tree)
    else:
        raise ValueError("Unable to read the gh-pages publishing branch")
    with tempfile.TemporaryDirectory() as directory:
        site = Path(directory)
        extract_site(archive, site)
        replace_site(tree, site, channel)
    git("config", "user.name", "github-actions[bot]", cwd=tree)
    git("config", "user.email", "41898282+github-actions[bot]@users.noreply.github.com", cwd=tree)
    git("add", "--all", cwd=tree)
    if subprocess.run(["git", "diff", "--cached", "--quiet"], cwd=tree).returncode == 0:
        print("Pages contents are already up to date")
        return
    git("commit", "-m", f"Deploy {channel} from {os.environ['GITHUB_SHA']}", cwd=tree)
    git("push", "origin", "HEAD:gh-pages", cwd=tree)


if __name__ == "__main__":
    main()
