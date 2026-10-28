# Contributing

Bug reports and focused fixes are welcome. Please describe what happened, what you expected, and how someone else can reproduce the problem. For a performance report, describe the route you played and whether the sound stayed clear.

## Before submitting a change

Run the source tests:

```sh
bash tools/test.sh
```

For target-code changes, also build with `bash build.sh` and test on a Vita. Mention what you tested and anything you could not check. Keep performance experiments separate from unrelated fixes so they can be compared fairly.

Keep support for the original APK and OBBs. Do not make a fresh installation depend on files generated on your own computer. Preserve existing author and license notices, and identify the source of any third-party code you add.

## Files to leave out

Do not submit game files, saves, extracted libraries, compiled packages, credentials, core dumps or private logs. Everything in `work/` is local and ignored. You can review the files selected by the repository's ignore rules with:

```sh
python3 tools/check_public_tree.py --list
```

This check is a useful first pass, not a substitute for reviewing the files yourself. Never force-add `work/` or game data. See [third-party notices](docs/THIRD_PARTY_NOTICES.md) for the included libraries.
