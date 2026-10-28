#!/usr/bin/env python3
"""Inventory the adjacent Gangstar Rio APK/OBBs and extract native ELF files."""
from __future__ import annotations

import argparse
import hashlib
import json
from pathlib import Path
import re
import subprocess
import zipfile

ROOT = Path(__file__).resolve().parents[1]
INPUTS = ROOT.parent
WORK = ROOT / "work"
APK = INPUTS / "Gangstar Rio City of Saints v1.2.1.apk"
OBB_DIR = INPUTS / "com.gameloft.android.ANMP.GloftG4HM"


def sha256(path: Path) -> str:
    digest = hashlib.sha256()
    with path.open("rb") as source:
        for block in iter(lambda: source.read(1024 * 1024), b""):
            digest.update(block)
    return digest.hexdigest()


def readelf(path: Path) -> dict:
    output = subprocess.run(
        ["readelf", "-h", "-A", "-d", "-Ws", str(path)],
        text=True, capture_output=True, check=True,
    ).stdout
    (WORK / f"{path.name}.readelf.txt").write_text(output)
    imports: set[str] = set()
    exports: set[str] = set()
    needed: list[str] = []
    for line in output.splitlines():
        match = re.search(r"Shared library: \[(.*?)\]", line)
        if match:
            needed.append(match.group(1))
        fields = line.split()
        if len(fields) >= 8 and fields[0].rstrip(":").isdigit():
            (imports if fields[6] == "UND" else exports).add(fields[7])
    return {
        "size": path.stat().st_size,
        "sha256": sha256(path),
        "needed": sorted(set(needed)),
        "import_count": len(imports),
        "imports": sorted(imports),
        "export_count": len(exports),
        "jni_exports": sorted(symbol for symbol in exports
                               if symbol.startswith("Java_") or symbol == "JNI_OnLoad"),
    }


def main() -> None:
    WORK.mkdir(parents=True, exist_ok=True)
    report: dict = {"apk": {}, "native_libraries": {}, "obbs": []}
    report["apk"] = {"name": APK.name, "size": APK.stat().st_size, "sha256": sha256(APK)}
    with zipfile.ZipFile(APK) as archive:
        entries = archive.infolist()
        report["apk"]["entry_count"] = len(entries)
        report["apk"]["native_library_entries"] = [
            {"name": item.filename, "size": item.file_size} for item in entries
            if item.filename.startswith("lib/") and item.filename.endswith(".so")
        ]
        for item in entries:
            if item.filename.startswith("lib/armeabi-v7a/") and item.filename.endswith(".so"):
                target = WORK / Path(item.filename).name
                target.write_bytes(archive.read(item))
                report["native_libraries"][target.name] = readelf(target)
    for path in sorted(OBB_DIR.glob("*.obb")):
        with path.open("rb") as source:
            first = source.read(32)
        report["obbs"].append({
            "name": path.name, "size": path.stat().st_size,
            "sha256": sha256(path), "zip_archive": zipfile.is_zipfile(path),
            "first_32_hex": first.hex(),
        })
    path = WORK / "inventory.json"
    path.write_text(json.dumps(report, indent=2) + "\n")
    print(f"APK SHA-256: {report['apk']['sha256']}")
    for name, library in report["native_libraries"].items():
        print(f"{name}: {library['size']} bytes; {library['import_count']} imports; "
              f"{library['export_count']} exports; JNI exports={len(library['jni_exports'])}")
    for obb in report["obbs"]:
        print(f"{obb['name']}: {obb['size']} bytes; ZIP={obb['zip_archive']}; SHA-256={obb['sha256']}")
    print(f"Report: {path}")


if __name__ == "__main__":
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--input-dir", type=Path, default=INPUTS)
    args = parser.parse_args()
    APK = args.input_dir / APK.name
    OBB_DIR = args.input_dir / OBB_DIR.name
    main()
