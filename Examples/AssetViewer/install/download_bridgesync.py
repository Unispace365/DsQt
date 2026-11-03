"""
Download the latest BridgeSync release from GitHub (private repo)
and place the exe into the bridgesync/ folder next to this script.

Requires: pip install -r requirements.txt

Usage:
    python download_bridgesync.py              # normal download
    python download_bridgesync.py --reset-token  # clear stored PAT and re-prompt
"""

import argparse
import os
import shutil
import sys

import keyring
import requests

GITHUB_REPO = "Unispace365/bridge-sync"
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


def find_exe_asset(release):
    """Find the first .exe asset in the release."""
    for asset in release.get("assets", []):
        if asset["name"].lower().endswith(".exe"):
            return asset
    print("Error: no .exe asset found in the latest release.")
    print("Assets found:", [a["name"] for a in release.get("assets", [])])
    sys.exit(1)


def download_asset(token, asset, dest_path):
    """Download a release asset directly to a file."""
    url = asset["url"]
    headers = {
        "Authorization": f"token {token}",
        "Accept": "application/octet-stream",
    }
    print(f"Downloading {asset['name']} ({asset['size'] / 1024 / 1024:.1f} MB)...")
    resp = requests.get(url, headers=headers, stream=True)
    resp.raise_for_status()

    downloaded = 0
    with open(dest_path, "wb") as f:
        for chunk in resp.iter_content(chunk_size=8192):
            f.write(chunk)
            downloaded += len(chunk)
            pct = downloaded / asset["size"] * 100
            print(f"\r  {pct:5.1f}%", end="", flush=True)
    print()


def main():
    parser = argparse.ArgumentParser(description="Download latest BridgeSync from GitHub.")
    parser.add_argument(
        "--reset-token",
        action="store_true",
        help="Clear the stored GitHub PAT and prompt for a new one.",
    )
    args = parser.parse_args()

    if args.reset_token:
        reset_token()

    script_dir = os.path.dirname(os.path.abspath(__file__))
    bridgesync_dir = os.path.join(script_dir, "bridgesync")

    token = get_token()
    release = get_latest_release(token)
    tag = release.get("tag_name", "unknown")
    print(f"Latest release: {tag}")

    asset = find_exe_asset(release)

    # Clean and recreate bridgesync folder
    if os.path.isdir(bridgesync_dir):
        print("Removing old bridgesync folder...")
        shutil.rmtree(bridgesync_dir)
    os.makedirs(bridgesync_dir, exist_ok=True)

    dest_path = os.path.join(bridgesync_dir, asset["name"])
    download_asset(token, asset, dest_path)

    # Verify
    if os.path.isfile(dest_path):
        print(f"Success! Saved to bridgesync/{asset['name']}")
    else:
        print("Uh oh, something went wrong!")
        sys.exit(1)


if __name__ == "__main__":
    main()
