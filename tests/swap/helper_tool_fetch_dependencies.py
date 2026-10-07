#!/usr/bin/env python3
"""Fetch the applications needed by the swap tests (Exchange as main app, Ethereum as library).

Each dependency listed in `[pytest.swap.dependencies]` of `ledger_app.toml` publishes a rolling
GitHub pre-release tagged `test-binaries`. It holds one zip per use case (`<use_case>.zip`) with the
`build/<device>/bin/app.elf` files of all devices. The zip is extracted where ragger expects it:
`.test_dependencies/main/<repo>/` for Exchange, `.test_dependencies/libraries/<repo>/` otherwise.

Nothing is built. It needs Python 3.11 or newer and the standard library only: it runs on the host or
inside the Docker image.
For private repositories, set GH_TOKEN (or GITHUB_TOKEN), or log in with `gh auth login`.
"""

import json
import os
import re
import shutil
import subprocess
import sys
import tempfile
import tomllib
import urllib.error
import urllib.request
import zipfile
from io import BytesIO
from pathlib import Path

RELEASE_TAG = "test-binaries"
MAIN_APP_REPO = re.compile(r"^app-exchange(-dev)?$")

APP_DIR = Path(__file__).parent.parent.parent.resolve()
BASE_DIR = Path(__file__).parent.resolve() / ".test_dependencies"
COMMIT_FILE = ".test-binaries-commit"
TIMEOUT_SECONDS = 60


def get_token() -> str | None:
    token = os.environ.get("GH_TOKEN") or os.environ.get("GITHUB_TOKEN")
    if token:
        return token
    try:
        out = subprocess.run(["gh", "auth", "token"], capture_output=True, text=True, timeout=5, check=False)
        return out.stdout.strip() or None
    except (OSError, subprocess.SubprocessError):
        return None


def github(api_path: str, token: str | None, accept: str = "application/vnd.github+json") -> bytes:
    headers = {"Accept": accept}
    if token:
        headers["Authorization"] = f"Bearer {token}"
    request = urllib.request.Request(f"https://api.github.com/{api_path}", headers=headers)
    with urllib.request.urlopen(request, timeout=TIMEOUT_SECONDS) as response:
        return response.read()


def read_dependencies() -> list[tuple[str, str]]:
    """Return the (repository slug, use case) of the swap dependencies of ledger_app.toml."""
    with open(APP_DIR / "ledger_app.toml", "rb") as f:
        manifest = tomllib.load(f)
    deps = manifest["pytest"]["swap"]["dependencies"]
    return [
        ("/".join(dep["url"].removesuffix(".git").split("/")[-2:]), dep["use_case"])
        for group in deps.values()
        for dep in group
    ]


def extract(content: bytes, dest: Path) -> None:
    """Extract the zip in a temporary directory, then replace `dest`, so no stale file remains."""
    dest.parent.mkdir(parents=True, exist_ok=True)
    with tempfile.TemporaryDirectory(dir=dest.parent, prefix=f".{dest.name}-") as tmp:
        staging = Path(tmp) / "content"
        with zipfile.ZipFile(BytesIO(content)) as archive:
            for name in archive.namelist():
                if not (staging / name).resolve().is_relative_to(staging.resolve()):
                    raise ValueError(f"Unsafe path in zip: {name}")
            archive.extractall(staging)
        if dest.exists():
            shutil.rmtree(dest)
        staging.rename(dest)


def fetch(repo_slug: str, use_case: str, token: str | None) -> None:
    repo_name = repo_slug.rsplit("/", maxsplit=1)[-1]
    dest = BASE_DIR / ("main" if MAIN_APP_REPO.match(repo_name) else "libraries") / repo_name
    asset_name = f"{use_case}.zip"

    # The release tag points to the commit the binaries were built from.
    commit = json.loads(github(f"repos/{repo_slug}/git/ref/tags/{RELEASE_TAG}", token))["object"]["sha"]
    release = json.loads(github(f"repos/{repo_slug}/releases/tags/{RELEASE_TAG}", token))
    asset = next((a for a in release["assets"] if a["name"] == asset_name), None)
    if asset is None:
        raise ValueError(f"asset {asset_name} not found in the '{RELEASE_TAG}' release of {repo_slug}")
    fetched = f"{asset_name}@{commit}@{asset['id']}"
    commit_file = dest / COMMIT_FILE
    if commit_file.exists() and commit_file.read_text() == fetched:
        print(f"{repo_slug}: {asset_name} is up to date ({commit[:8]})")
        return

    print(f"{repo_slug}: downloading {asset_name} ({commit[:8]}) into {dest.relative_to(APP_DIR)}")
    extract(github(f"repos/{repo_slug}/releases/assets/{asset['id']}", token, "application/octet-stream"), dest)
    commit_file.write_text(fetched)


def main() -> int:
    token = get_token()
    failed = False
    for repo_slug, use_case in read_dependencies():
        try:
            fetch(repo_slug, use_case, token)
        except (urllib.error.URLError, ValueError, zipfile.BadZipFile, OSError) as error:
            hint = (
                " (private repository? set GH_TOKEN or run 'gh auth login')"
                if isinstance(error, urllib.error.HTTPError)
                else ""
            )
            print(f"{repo_slug}: failed: {error}{hint}", file=sys.stderr)
            failed = True
    return 1 if failed else 0


if __name__ == "__main__":
    sys.exit(main())
