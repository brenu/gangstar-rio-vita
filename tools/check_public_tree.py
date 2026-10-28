#!/usr/bin/env python3
"""Audit files selected by repository ignore rules; never stage, commit or upload."""
import argparse
import json
from pathlib import Path
import subprocess
import tempfile

ROOT = Path(__file__).resolve().parents[1]
FORBIDDEN = {'.apk', '.aab', '.obb', '.so', '.a', '.o', '.obj', '.elf', '.velf', '.self', '.vpk', '.core', '.dmp', '.psp2dmp', '.log', '.pyc'}


def inventory():
    # An empty temporary index makes this useful before the real repo exists.
    # No files or Git metadata are changed in the project itself.
    with tempfile.TemporaryDirectory(prefix='rio-public-audit-') as folder:
        subprocess.run(['git', 'init', '-q', folder], check=True)
        result = subprocess.run(['git', '-c', 'core.excludesFile=/dev/null',
            f'--git-dir={folder}/.git', f'--work-tree={ROOT}', 'ls-files',
            '--others', '--exclude-standard', '-z'], check=True, capture_output=True)
    return sorted(name for name in result.stdout.decode().split('\0') if name)


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('--list', action='store_true', help='print every shareable relative path')
    args = parser.parse_args()
    files = inventory()
    problems = []
    for name in files:
        path = ROOT / name
        if path.is_symlink():
            problems.append(f'Symlink requires manual review: {name}')
            continue
        if path.suffix.lower() in FORBIDDEN or name.startswith(('work/', 'build/', 'staging/', 'private/', 'game-data/')):
            problems.append(f'Private/generated file selected: {name}')
        if path.stat().st_size > 10 * 1024 * 1024:
            problems.append(f'Large file requires manual review: {name}')
    required = ['README.md', 'LICENSE', '.gitignore', 'CMakeLists.txt', 'build.sh', 'config/tested.sh', 'docs/THIRD_PARTY_NOTICES.md']
    for name in required:
        if name not in files:
            problems.append(f'Required source file missing or ignored: {name}')
    report = {'files': files, 'file_count': len(files),
        'bytes': sum((ROOT / name).stat().st_size for name in files), 'problems': problems,
        'note': 'Ignore/inventory checks do not establish copyright provenance or detect every secret.'}
    out = ROOT / 'work/public-tree-report.json'
    out.parent.mkdir(parents=True, exist_ok=True)
    out.write_text(json.dumps(report, indent=2) + '\n')
    if args.list:
        print('\n'.join(files))
    print(f'Shareable inventory: {len(files)} files, {report["bytes"]:,} bytes; {len(problems)} problems')
    for problem in problems:
        print(problem)
    print(f'Local report: {out}')
    raise SystemExit(bool(problems))


if __name__ == '__main__':
    main()
