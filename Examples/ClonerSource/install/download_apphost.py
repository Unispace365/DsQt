"""
Download the latest DSAppHost release from GitHub (private repo)
and extract it into the DSAppHost/ folder next to this script.

Requires: pip install -r requirements.txt

Usage:
    python download_apphost.py              # normal download
    python download_apphost.py --reset-token  # clear stored PAT and re-prompt
"""

import argparse
import io
import os
import shutil
import sys
import zipfile

import keyring
import requests

GITHUB_REPO = "Unispace365/DSAppHost"
KEYRING_SERVICE = "github-dsapphost"
KEYRING_USERNAME = "pat"


def reset_token():
    """Remove the stored PAT from Windows Credential Manager."""
    try:
        keyring.delete_password(KEYRING_SERVICE, KEYRING_USERNAME)
        print("Stored token removed.")
    except keyring.errors.PasswordDeleteError:
        print("No stored token found.")


def get_token():
    """Retrieve the GitHub PAT from the Windows Credential Manager, prompting if missing."""
    token = keyring.get_password(KEYRING_SERVICE, KEYRING_USERNAME)
    if token:
        return token

    print("No GitHub PAT found in Windows Credential Manager.")
    token = input("Enter your GitHub Personal Access Token: ").strip()
    if not token:
        print("Error: token cannot be empty.")
        sys.exit(1)

    store = input("Save this token for future use? [Y/n]: ").strip().lower()
    if store != "n":
        keyring.set_password(KEYRING_SERVICE, KEYRING_USERNAME, token)
        print("Token saved to Windows Credential Manager.")
    return token


def get_latest_release(token):
    """Get the latest release metadata from the GitHub API."""
    url = f"https://api.github.com/repos/{GITHUB_REPO}/releases/latest"
    headers = {
        "Authorization": f"token {token}",
        "Accept": "application/vnd.github+json",
    }
    resp = requests.get(url, headers=headers)
    if resp.status_code == 401:
        print("Error: authentication failed. Your PAT may be invalid or expired.")
        clear = input("Remove stored token and re-enter? [Y/n]: ").strip().lower()
        if clear != "n":
            keyring.delete_password(KEYRING_SERVICE, KEYRING_USERNAME)
        sys.exit(1)
    resp.raise_for_status()
    return resp.json()


def find_zip_asset(release):
    """Find the first .zip asset in the release."""
    for asset in release.get("assets", []):
        if asset["name"].lower().endswith(".zip"):
            return asset
    print("Error: no .zip asset found in the latest release.")
    print("Assets found:", [a["name"] for a in release.get("assets", [])])
    sys.exit(1)


def download_asset(token, asset):
    """Download a release asset and return its bytes."""
    url = asset["url"]
    headers = {
        "Authorization": f"token {token}",
        "Accept": "application/octet-stream",
    }
    print(f"Downloading {asset['name']} ({asset['size'] / 1024 / 1024:.1f} MB)...")
    resp = requests.get(url, headers=headers, stream=True)
    resp.raise_for_status()

    chunks = []
    downloaded = 0
    for chunk in resp.iter_content(chunk_size=8192):
        chunks.append(chunk)
        downloaded += len(chunk)
        pct = downloaded / asset["size"] * 100
        print(f"\r  {pct:5.1f}%", end="", flush=True)
    print()
    return b"".join(chunks)


def extract_zip(zip_bytes, dest_dir):
    """Extract zip contents into dest_dir, handling both flat and nested layouts."""
    with zipfile.ZipFile(io.BytesIO(zip_bytes)) as zf:
        # Check if all entries share a common top-level folder
        names = zf.namelist()
        top_dirs = {n.split("/")[0] for n in names if "/" in n}

        if len(top_dirs) == 1:
            # Zip has a single top-level folder — strip it so contents go directly into dest_dir
            prefix = top_dirs.pop() + "/"
            for info in zf.infolist():
                if info.filename == prefix or not info.filename.startswith(prefix):
                    continue
                # Rewrite the extraction path to remove the top-level folder
                relative = info.filename[len(prefix):]
                if not relative:
                    continue
                target = os.path.join(dest_dir, relative)
                if info.is_dir():
                    os.makedirs(target, exist_ok=True)
                else:
                    os.makedirs(os.path.dirname(target), exist_ok=True)
                    with zf.open(info) as src, open(target, "wb") as dst:
                        shutil.copyfileobj(src, dst)
        else:
            # Flat zip — extract directly
            zf.extractall(dest_dir)


def main():
    parser = argparse.ArgumentParser(description="Download latest DSAppHost from GitHub.")
    parser.add_argument(
        "--reset-token",
        action="store_true",
        help="Clear the stored GitHub PAT and prompt for a new one.",
    )
    args = parser.parse_args()

    if args.reset_token:
        reset_token()

    script_dir = os.path.dirname(os.path.abspath(__file__))
    apphost_dir = os.path.join(script_dir, "DSAppHost")

    token = get_token()
    release = get_latest_release(token)
    tag = release.get("tag_name", "unknown")
    print(f"Latest release: {tag}")

    asset = find_zip_asset(release)

    # Remove old DSAppHost folder
    if os.path.isdir(apphost_dir):
        print("Removing old DSAppHost folder...")
        shutil.rmtree(apphost_dir)

    zip_bytes = download_asset(token, asset)

    print("Extracting...")
    os.makedirs(apphost_dir, exist_ok=True)
    extract_zip(zip_bytes, apphost_dir)

    # Verify
    if os.path.isdir(apphost_dir) and os.listdir(apphost_dir):
        print("Success!")
    else:
        print("Uh oh, something went wrong!")
        sys.exit(1)


if __name__ == "__main__":
    main()
