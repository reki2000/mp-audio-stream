"""Check release archive contents and reject incomplete or misidentified builds."""

import os
from pathlib import Path
import tarfile
import tempfile
import unittest
from unittest.mock import patch
import zipfile

from release import metadata, package, windows_runtime
from publish import TARGETS, select_run, verify_assets
import hashlib
import json


class ReleaseTests(unittest.TestCase):
    def setUp(self):
        self.temporary = tempfile.TemporaryDirectory()
        self.addCleanup(self.temporary.cleanup)
        self.root = Path(self.temporary.name)
        (self.root / "pubspec.yaml").write_text("version: 1.2.3+4\n")
        self.environment = patch.dict(os.environ, {"APP_DIR": "example", "ARTIFACT_NAME": "demo",
                                     "VERSION_FILE": "pubspec.yaml", "GITHUB_REF": "refs/tags/v1.2.3",
                                     "GITHUB_RUN_NUMBER": "12"}, clear=True)
        self.environment.start()
        self.addCleanup(self.environment.stop)

    def file(self, path, data=b"built output"):
        path = self.root / path
        path.parent.mkdir(parents=True, exist_ok=True)
        path.write_bytes(data)
        return path

    def test_tag_must_identify_the_package_version(self):
        self.assertEqual(metadata(self.root)["build_name"], "1.2.3")
        os.environ["GITHUB_REF"] = "refs/tags/v1.2.4"
        with self.assertRaises(ValueError):
            metadata(self.root)

    def test_prerelease_uses_a_numeric_mobile_version(self):
        (self.root / "pubspec.yaml").write_text("version: 1.2.3-rc.1+4\n")
        os.environ["GITHUB_REF"] = "refs/tags/v1.2.3-rc.1"
        self.assertEqual(metadata(self.root)["version"], "1.2.3-rc.1")
        self.assertEqual(metadata(self.root)["build_name"], "1.2.3")

    def test_linux_archive_preserves_executable_and_libraries(self):
        binary = self.file("example/build/linux/arm64/release/bundle/demo")
        binary.chmod(0o755)
        self.file("example/build/linux/arm64/release/bundle/data/icudtl.dat")
        self.file("example/build/linux/arm64/release/bundle/lib/plugin.so")
        package(self.root, "linux", "arm64")
        with tarfile.open(next((self.root / "release-assets").glob("*.tar.gz"))) as archive:
            self.assertEqual(archive.getmember("demo/demo").mode & 0o111, 0o111)
            self.assertIn("demo/lib/plugin.so", archive.getnames())

    def test_web_archive_contains_wasm_and_javascript_fallback(self):
        for name in ("index.html", "main.dart.wasm", "main.dart.mjs", "main.dart.js", "assets/example.bin"):
            self.file(f"example/build/web/{name}")
        package(self.root, "web", "browser")
        with zipfile.ZipFile(next((self.root / "release-assets").glob("*.zip"))) as archive:
            self.assertIn("index.html", archive.namelist())
            self.assertIn("assets/example.bin", archive.namelist())
            self.assertIn("main.dart.js", archive.namelist())

    def test_incomplete_web_build_is_not_packaged(self):
        self.file("example/build/web/index.html")
        with self.assertRaises(ValueError):
            package(self.root, "web", "browser")

    def test_missing_android_abi_fails(self):
        self.file("example/build/app/outputs/flutter-apk/app-arm64-v8a-release.apk")
        with self.assertRaises(ValueError):
            package(self.root, "android", "all")

    def test_windows_runtime_uses_the_newest_matching_architecture(self):
        bundle = self.root / "bundle"
        bundle.mkdir()
        for version in ("14.9.0", "14.40.0"):
            for name in ("msvcp140.dll", "vcruntime140.dll"):
                self.file(f"VS/VC/Redist/MSVC/{version}/arm64/Microsoft.VC143.CRT/{name}", version.encode())
        windows_runtime(bundle, "arm64", self.root / "VS")
        self.assertEqual((bundle / "msvcp140.dll").read_bytes(), b"14.40.0")
        with self.assertRaises(ValueError):
            windows_runtime(bundle, "x64", self.root / "VS")

    def test_release_requires_an_exact_successful_branch_build(self):
        runs = [{"id": 7, "head_sha": "other", "status": "completed", "conclusion": "success", "event": "push"},
                {"id": 8, "head_sha": "verified", "status": "completed", "conclusion": "failure", "event": "push"},
                {"id": 9, "head_sha": "verified", "status": "completed", "conclusion": "success", "event": "pull_request"},
                {"id": 10, "head_sha": "verified", "status": "completed", "conclusion": "success", "event": "push"}]
        self.assertEqual(select_run(runs, "verified")["id"], 10)
        with self.assertRaises(ValueError):
            select_run(runs, "verified", "9")
        with self.assertRaises(ValueError):
            select_run(runs, "missing")

    def test_release_checks_every_target_and_binary_digest(self):
        for target, arch in TARGETS:
            name = f"demo-{target}-{arch}.zip"
            asset = self.file(f"downloads/{target}-{arch}/{name}")
            manifest = {"target": target, "architecture": arch, "commit": "verified",
                        "run_id": "10", "version": "1.2.3",
                        "assets": {name: hashlib.sha256(asset.read_bytes()).hexdigest()}}
            self.file(f"downloads/{target}-{arch}/manifest-{target}-{arch}.json", json.dumps(manifest).encode())
        directory = self.root / "downloads"
        manifests, assets = verify_assets(directory, "verified", 10, "1.2.3")
        self.assertEqual(len(manifests), len(TARGETS))
        self.assertEqual(len(assets), len(TARGETS))
        next(directory.rglob("*.zip")).write_bytes(b"modified")
        with self.assertRaises(ValueError):
            verify_assets(directory, "verified", 10, "1.2.3")

    def test_release_rejects_expired_or_missing_artifacts(self):
        directory = self.root / "downloads"
        directory.mkdir()
        with self.assertRaises(ValueError):
            verify_assets(directory, "verified", 10, "1.2.3")


if __name__ == "__main__":
    unittest.main()
