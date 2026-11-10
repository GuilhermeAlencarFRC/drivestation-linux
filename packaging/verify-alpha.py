#!/usr/bin/env python3
"""Verify a built alpha archive and exercise its extracted executables (Python >= 3.12)."""

import argparse
import hashlib
import json
import os
from pathlib import Path, PurePosixPath
import re
import subprocess
import tarfile


def require(condition, message):
    if not condition:
        raise RuntimeError(message)


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("archive", type=Path)
    parser.add_argument("destination", type=Path, help="New directory; existing paths are preserved")
    args = parser.parse_args()
    archive = args.archive.resolve(strict=True)
    destination = args.destination.absolute()
    require(archive.name.endswith("-TESTES.tar.gz"), "Archive must be labeled TESTES")
    prefix = archive.name.removesuffix(".tar.gz")
    digest = hashlib.sha256(archive.read_bytes()).hexdigest()
    expected_digest = Path(str(archive) + ".sha256").read_text().split()[0]
    require(digest == expected_digest, "Archive SHA256 mismatch")
    expected_files = {
        "LEIA-ME-ALFA.txt", "INICIAR-ALFA.sh", "build-info.json",
        "bin/drivestation-linux", "bin/drivestation-network-check",
        "share/applications/io.github.GuilhermeAlencarFRC.DriveStationLinux.desktop",
        "share/icons/hicolor/scalable/apps/drivestation-linux.svg",
        "share/doc/DriveStationLinux/README.md",
    }
    with tarfile.open(archive, "r:gz") as package:
        members = package.getmembers()
        require(sum(member.size for member in members) < 50 * 1024 * 1024,
                "Unexpected archive size")
        actual_files = set()
        for member in members:
            path = PurePosixPath(member.name)
            require(not path.is_absolute() and ".." not in path.parts and path.parts[0] == prefix,
                    "Unexpected archive path")
            require(member.isfile() or member.isdir(), "Archive links/devices are not supported")
            if member.isfile():
                actual_files.add(str(PurePosixPath(*path.parts[1:])))
        require(actual_files == expected_files, f"Unexpected files: {actual_files ^ expected_files}")
        destination.mkdir(parents=True, exist_ok=False)
        package.extractall(destination, filter="data")
    root = destination / prefix
    metadata = json.loads((root / "build-info.json").read_text())
    version = metadata["version"]
    require(re.fullmatch(r"\d+\.\d+\.\d+-alpha\.\d+", version), "Missing alpha version")
    require(version in prefix, "Version differs from archive name")
    require(metadata["stage"] == "alpha" and metadata["physical_robot_validated"] is False,
            "Incorrect release status")
    require("VERSÃO DE TESTES" in (root / "LEIA-ME-ALFA.txt").read_text(), "Missing test notice")
    environment = dict(os.environ, QT_QPA_PLATFORM="offscreen")

    def run(command):
        return subprocess.run(command, env=environment, text=True, capture_output=True,
                              check=True, timeout=15).stdout

    output = run([str(root / "INICIAR-ALFA.sh"), "--version"])
    require(version in output and "VERSÃO DE TESTES" in output, "Launcher version/status mismatch")
    check = json.loads(run([str(root / "bin/drivestation-network-check"), "--json"]))
    require(check["success"] and check["clock_verified"], "Installed loopback check failed")
    require(check["version"] == version, "Executable version mismatch")
    screenshot = destination / "alpha-preview.png"
    run([str(root / "INICIAR-ALFA.sh"), "--config", str(destination / "isolated-settings.ini"),
         "--screenshot", str(screenshot)])
    require(screenshot.is_file() and screenshot.stat().st_size > 0, "Preview not produced")
    print(json.dumps({"success": True, "version": version, "stage": "alpha-test",
                      "archive": str(archive), "sha256": digest, "files": sorted(actual_files),
                      "loopback": check, "preview": str(screenshot)}, ensure_ascii=False, indent=2))


if __name__ == "__main__":
    main()
