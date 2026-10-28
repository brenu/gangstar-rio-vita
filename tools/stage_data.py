#!/usr/bin/env python3
"""Stage the supported Gangstar Rio revision; preserve both original OBBs."""
import argparse
import hashlib
from pathlib import Path
import shutil
import zipfile

ROOT = Path(__file__).resolve().parents[1]
INPUTS = {
    'Gangstar Rio City of Saints v1.2.1.apk': '32f7cd2c09e2c1db248c8219afe8af56ce5e81bc22dd5f5148d0c690aae87cd9',
    'com.gameloft.android.ANMP.GloftG4HM/main.12030.com.gameloft.android.ANMP.GloftG4HM.obb': '869dc31388c8274dc34e83a592d1f8530d8e82e4970d5279e8b4587fb144ce37',
    'com.gameloft.android.ANMP.GloftG4HM/patch.12136.com.gameloft.android.ANMP.GloftG4HM.obb': 'a29642259a0bf757ce742b66ca7c96c7ea28a655ff912ae7fbcb18f62f04bcc4',
}


def sha(path):
    digest = hashlib.sha256()
    with path.open('rb') as stream:
        for block in iter(lambda: stream.read(1024 * 1024), b''):
            digest.update(block)
    return digest.hexdigest()


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('--dry-run', action='store_true', help='verify inputs and list copies without writing game data')
    parser.add_argument('--input-dir', type=Path, default=ROOT.parent)
    parser.add_argument('--output-dir', type=Path, default=ROOT / 'work/staging/ux0/data/gangstarrio')
    args = parser.parse_args()
    copies = []
    for index, (name, expected) in enumerate(INPUTS.items()):
        source = args.input_dir / name
        if sha(source) != expected:
            raise SystemExit(f'Revision mismatch: {source}; reanalyze before staging.')
        destination = args.output_dir / ('game.apk' if index == 0 else source.name)
        copies.append((source, destination))
    with zipfile.ZipFile(copies[0][0]) as apk:
        native = [name for name in apk.namelist() if name.startswith('lib/armeabi-v7a/') and name.endswith('.so')]
        if 'lib/armeabi-v7a/libgl2jni.so' not in native:
            raise SystemExit('Gangstar ARMv7 game library missing from APK')
        for source, destination in copies:
            if source == copies[0][0]:
                continue  # APK is verified/read locally; only the two libraries go on Vita.
            print(f'{source} -> {destination}')
            if not args.dry_run:
                destination.parent.mkdir(parents=True, exist_ok=True)
                if not destination.exists() or sha(destination) != sha(source):
                    shutil.copyfile(source, destination)
        for name in native:
            destination = args.output_dir / Path(name).name
            print(f'{name} -> {destination}')
            if not args.dry_run:
                destination.write_bytes(apk.read(name))
    print('Verified original Gangstar files. Vita destination: ux0:/data/gangstarrio/')


if __name__ == '__main__':
    main()
