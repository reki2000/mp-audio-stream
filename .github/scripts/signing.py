"""Install optional Android or iOS signing material on an ephemeral runner."""

import base64
import os
from pathlib import Path
import plistlib
import secrets
import subprocess
import sys


def export(values):
    with open(os.environ["GITHUB_ENV"], "a") as output:
        for key, value in values.items():
            if "\n" in value or "\r" in value:
                raise ValueError(f"Invalid multiline signing setting: {key}")
            output.write(f"{key}={value}\n")


def decode(name, path):
    path.write_bytes(base64.b64decode(os.environ[name], validate=True))
    path.chmod(0o600)
    return str(path)


target = sys.argv[1]
required = {
    "android": ("ANDROID_KEYSTORE_BASE64", "ANDROID_KEYSTORE_PASSWORD", "ANDROID_KEY_ALIAS", "ANDROID_KEY_PASSWORD"),
    "ios": ("IOS_CERTIFICATE_BASE64", "IOS_CERTIFICATE_PASSWORD", "IOS_PROFILE_BASE64", "IOS_EXPORT_OPTIONS_BASE64"),
}[target]
present = [bool(os.environ.get(name)) for name in required]
if not any(present):
    print(f"No {target} signing material: using the documented unsigned/test-signing mode.")
    sys.exit(0)
if not all(present):
    raise ValueError(f"Configure all signing secrets together: {', '.join(required)}")

temporary = Path(os.environ["RUNNER_TEMP"])
if target == "android":
    path = decode("ANDROID_KEYSTORE_BASE64", temporary / "release.keystore")
    export({"ANDROID_KEYSTORE_PATH": path})
else:
    certificate = decode("IOS_CERTIFICATE_BASE64", temporary / "signing.p12")
    profile = decode("IOS_PROFILE_BASE64", temporary / "signing.mobileprovision")
    options = decode("IOS_EXPORT_OPTIONS_BASE64", temporary / "ExportOptions.plist")
    plistlib.loads(Path(options).read_bytes())
    keychain = str(temporary / "app-signing.keychain-db")
    password = secrets.token_hex(24)
    print(f"::add-mask::{password}")
    def security(*args):
        result = subprocess.run(["security", *args], capture_output=True)
        if result.returncode:
            raise RuntimeError(f"security {args[0]} failed")
        return result.stdout
    security("create-keychain", "-p", password, keychain)
    security("set-keychain-settings", "-lut", "21600", keychain)
    security("unlock-keychain", "-p", password, keychain)
    security("import", certificate, "-P", os.environ["IOS_CERTIFICATE_PASSWORD"], "-A", "-t", "cert", "-f", "pkcs12", "-k", keychain)
    security("set-key-partition-list", "-S", "apple-tool:,apple:", "-k", password, keychain)
    security("list-keychains", "-d", "user", "-s", keychain, str(Path.home() / "Library/Keychains/login.keychain-db"))
    info = plistlib.loads(security("cms", "-D", "-i", profile))
    profiles = Path.home() / "Library/MobileDevice/Provisioning Profiles"
    profiles.mkdir(parents=True, exist_ok=True)
    installed = profiles / f"{info['UUID']}.mobileprovision"
    installed.write_bytes(Path(profile).read_bytes())
    # Match the app's bundle identifier to the profile, rather than editing source files.
    identifier = info["Entitlements"]["application-identifier"].split(".", 1)[1]
    if "*" in identifier:
        raise ValueError("Use an explicit iOS application provisioning profile")
    export({"IOS_EXPORT_OPTIONS_PATH": options,
            "FLUTTER_XCODE_DEVELOPMENT_TEAM": info["TeamIdentifier"][0],
            "FLUTTER_XCODE_PRODUCT_BUNDLE_IDENTIFIER": identifier,
            "FLUTTER_XCODE_CODE_SIGN_STYLE": "Manual",
            "FLUTTER_XCODE_PROVISIONING_PROFILE_SPECIFIER": info["UUID"],
            "FLUTTER_XCODE_CODE_SIGN_IDENTITY": "Apple Distribution"})
