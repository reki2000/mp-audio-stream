"""Build and package a Flutter application for GitHub Releases."""

import argparse
import hashlib
import json
import os
from pathlib import Path
import re
import shutil
import subprocess
import tarfile
import tempfile
import zipfile


def version_from(path):
    match = re.search(r"^version:\s*['\"]?([^\s'\"#]+)", path.read_text(), re.M)
    if not match:
        raise ValueError(f"Missing version in {path}")
    version = match[1].split("+")[0]
    if not re.fullmatch(r"\d+\.\d+\.\d+(?:-[0-9A-Za-z.-]+)?", version):
        raise ValueError(f"Invalid release version: {version}")
    return version


def metadata(root):
    version = version_from(root / os.environ.get("VERSION_FILE", "pubspec.yaml"))
    tag = os.environ.get("GITHUB_REF", "")
    if tag.startswith("refs/tags/") and tag != f"refs/tags/v{version}":
        raise ValueError(f"Tag {tag} must match pubspec version v{version}")
    number = os.environ.get("GITHUB_RUN_NUMBER", "1")
    if not number.isdecimal() or int(number) < 1:
        raise ValueError("Build number must be a positive integer")
    return {"version": version, "build_name": version.split("-")[0], "build_number": number}


def run(*args, cwd=None, env=None):
    subprocess.run(args, cwd=cwd, env=env, check=True)


def zip_tree(source, destination, prefix=""):
    with zipfile.ZipFile(destination, "w", zipfile.ZIP_DEFLATED) as archive:
        for path in sorted(source.rglob("*")):
            if path.is_file():
                archive.write(path, str(Path(prefix) / path.relative_to(source)))


def require_file(path):
    if not path.is_file() or path.stat().st_size == 0:
        raise ValueError(f"Missing or empty build output: {path}")


def digest_file(path):
    with path.open("rb") as stream:
        return hashlib.file_digest(stream, "sha256").hexdigest()


def windows_runtime(bundle, arch, install):
    candidates = list((install / "VC/Redist/MSVC").glob(f"*/{arch}/Microsoft.VC*.CRT"))
    if not candidates:
        raise ValueError(f"Visual C++ runtime for {arch} was not found")
    latest = max(candidates, key=lambda p: tuple(int(x) for x in p.parent.parent.name.split(".")))
    for dll in latest.glob("*.dll"):
        shutil.copy2(dll, bundle / dll.name)
    for name in ("msvcp140.dll", "vcruntime140.dll"):
        require_file(bundle / name)


def package(root, target, arch):
    app = root / os.environ.get("APP_DIR", ".")
    output = root / "release-assets"
    output.mkdir(exist_ok=True)
    meta = metadata(root)
    name = f"{os.environ['ARTIFACT_NAME']}-{meta['version']}-{target}-{arch}"
    build = app / "build"

    if target == "android":
        signing = "signed" if os.environ.get("ANDROID_KEYSTORE_PATH") else "test-signed"
        for abi in ("armeabi-v7a", "arm64-v8a", "x86_64"):
            apk = build / f"app/outputs/flutter-apk/app-{abi}-release.apk"
            require_file(apk)
            shutil.copy2(apk, output / f"{name}-{abi}-{signing}.apk")
        bundle = build / "app/outputs/bundle/release/app-release.aab"
        require_file(bundle)
        shutil.copy2(bundle, output / f"{name}-{signing}.aab")
    elif target == "linux":
        bundle = build / f"linux/{arch}/release/bundle"
        require_file(bundle / "data/icudtl.dat")
        if not any(p.is_file() and os.access(p, os.X_OK) for p in bundle.iterdir()):
            raise ValueError("Linux bundle has no executable")
        with tarfile.open(output / f"{name}.tar.gz", "w:gz") as archive:
            archive.add(bundle, arcname=os.environ["ARTIFACT_NAME"])
    elif target == "windows":
        bundle = build / f"windows/{arch}/runner/Release"
        require_file(bundle / "flutter_windows.dll")
        require_file(bundle / "data/icudtl.dat")
        if not list(bundle.glob("*.exe")):
            raise ValueError("Windows bundle has no executable")
        vswhere = Path(os.environ["ProgramFiles(x86)"]) / "Microsoft Visual Studio/Installer/vswhere.exe"
        install = subprocess.check_output(
            [str(vswhere), "-latest", "-products", "*", "-property", "installationPath"], text=True
        ).strip()
        if not install:
            raise ValueError("Visual Studio installation was not found")
        windows_runtime(bundle, arch, Path(install))
        zip_tree(bundle, output / f"{name}.zip", os.environ["ARTIFACT_NAME"])
    elif target == "macos":
        apps = list((build / "macos/Build/Products/Release").glob("*.app"))
        if len(apps) != 1:
            raise ValueError(f"Expected one macOS app, found {len(apps)}")
        app_bundle = apps[0]
        plist = app_bundle / "Contents/Info.plist"
        import plistlib
        executable = plistlib.loads(plist.read_bytes())["CFBundleExecutable"]
        run("lipo", str(app_bundle / "Contents/MacOS" / executable), "-verify_arch", "x86_64", "arm64")
        for framework in ("App.framework/App", "FlutterMacOS.framework/FlutterMacOS"):
            run("lipo", str(app_bundle / "Contents/Frameworks" / framework), "-verify_arch", "x86_64", "arm64")
        args = ["codesign", "--force", "--deep", "--sign", "-"]
        entitlements = app / "macos/Runner/Release.entitlements"
        if entitlements.is_file():
            args += ["--entitlements", str(entitlements)]
        run(*args, str(app_bundle))
        run("codesign", "--verify", "--deep", "--strict", str(app_bundle))
        run("ditto", "-c", "-k", "--sequesterRsrc", "--keepParent", str(app_bundle), str(output / f"{name}-adhoc.zip"))
    elif target == "ios":
        if os.environ.get("IOS_EXPORT_OPTIONS_PATH"):
            ipas = list((build / "ios/ipa").glob("*.ipa"))
            if len(ipas) != 1:
                raise ValueError("Signed iOS export did not produce exactly one IPA")
            require_file(ipas[0])
            shutil.copy2(ipas[0], output / f"{name}-signed.ipa")
        else:
            apps = list((build / "ios/iphoneos").glob("*.app"))
            if len(apps) != 1:
                raise ValueError("Unsigned iOS build did not produce exactly one app")
            with tempfile.TemporaryDirectory() as directory:
                payload = Path(directory) / "Payload"
                payload.mkdir()
                run("ditto", str(apps[0]), str(payload / apps[0].name))
                run("ditto", "-c", "-k", "--keepParent", str(payload), str(output / f"{name}-unsigned.ipa"))
    elif target == "ios-simulator":
        apps = list((build / "ios/iphonesimulator").glob("*.app"))
        if len(apps) != 1:
            raise ValueError("Simulator build did not produce exactly one app")
        run("ditto", "-c", "-k", "--keepParent", str(apps[0]), str(output / f"{name}-debug.zip"))
    elif target == "web":
        for file in ("index.html", "main.dart.wasm", "main.dart.mjs", "main.dart.js"):
            require_file(build / "web" / file)
        zip_tree(build / "web", output / f"{name}.zip")
    else:
        raise ValueError(f"Unknown platform: {target}")
    assets = sorted(p for p in output.glob(f"{name}*") if p.is_file())
    if not assets:
        raise ValueError(f"No release assets for {target}/{arch}")
    manifest = {"version": meta["version"], "commit": os.environ.get("GITHUB_SHA", ""),
                "run_id": os.environ.get("GITHUB_RUN_ID", ""), "target": target, "architecture": arch,
                "assets": {p.name: digest_file(p) for p in assets}}
    (output / f"manifest-{target}-{arch}.json").write_text(json.dumps(manifest, indent=2) + "\n")


def build(root, target, arch):
    meta = metadata(root)
    app = root / os.environ.get("APP_DIR", ".")
    common = [f"--build-name={meta['build_name']}", f"--build-number={meta['build_number']}",
              f"--dart-define=APP_VERSION={meta['version']}"]
    flutter = shutil.which("flutter")
    if not flutter:
        raise ValueError("Flutter is not on PATH")
    if target == "android":
        run(flutter, "build", "apk", "--release", "--split-per-abi", *common, cwd=app)
        run(flutter, "build", "appbundle", "--release", *common, cwd=app)
    elif target == "ios":
        if os.environ.get("IOS_EXPORT_OPTIONS_PATH"):
            run(flutter, "build", "ipa", "--release", f"--export-options-plist={os.environ['IOS_EXPORT_OPTIONS_PATH']}", *common, cwd=app)
        else:
            run(flutter, "build", "ios", "--release", "--no-codesign", *common, cwd=app)
    elif target == "ios-simulator":
        run(flutter, "build", "ios", "--simulator", "--debug", "--no-codesign", *common, cwd=app)
    elif target == "macos":
        run(flutter, "config", "--no-enable-macos-arm64-only", cwd=app)
        env = dict(os.environ, FLUTTER_XCODE_CODE_SIGNING_ALLOWED="NO")
        run(flutter, "build", "macos", "--release", *common, cwd=app, env=env)
    elif target == "web":
        run(flutter, "build", "web", "--wasm", "--release", "--base-href=/", "--no-web-resources-cdn", *common, cwd=app)
    elif target in ("linux", "windows"):
        run(flutter, "build", target, "--release", *common, cwd=app)
    else:
        raise ValueError(f"Unknown platform: {target}")
    package(root, target, arch)


if __name__ == "__main__":
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("command", choices=("metadata", "build", "package"))
    parser.add_argument("target", nargs="?")
    parser.add_argument("arch", nargs="?")
    args = parser.parse_args()
    root = Path(__file__).resolve().parents[2]
    if args.command == "metadata":
        data = metadata(root)
        text = "".join(f"{key}={value}\n" for key, value in data.items())
        print(text, end="")
        if os.environ.get("GITHUB_OUTPUT"):
            with open(os.environ["GITHUB_OUTPUT"], "a") as stream:
                stream.write(text)
    else:
        if not args.target or not args.arch:
            parser.error("target and architecture are required")
        (build if args.command == "build" else package)(root, args.target, args.arch)
