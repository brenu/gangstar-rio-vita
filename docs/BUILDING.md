# Build the VPK

These instructions build the current preferred gameplay configuration. You do not need the Android game files until the installation step.

## 1. Prepare your computer

The build scripts use Bash. Linux is the tested build environment; on Windows, use WSL2. macOS users also need a working VitaSDK installation.

On Ubuntu or Debian, install the host tools:

```sh
sudo apt update
sudo apt install build-essential cmake git python3 zlib1g-dev
```

Install VitaSDK using the [official setup instructions](https://vitasdk.org/). Set the path to your installation:

```sh
export VITASDK=/usr/local/vitasdk
export PATH="$VITASDK/bin:$PATH"
```

If you installed it somewhere else, change the first line. Keep these variables set in every terminal used for building.

## 2. Check the Vita libraries

Open a terminal in the directory containing this repository's `build.sh`, then run:

```sh
python3 tools/preflight.py
```

A successful check ends with `"softfp_link_probe_passed": true` and `"missing": []`.

If it lists missing files, install the corresponding VitaSDK libraries before continuing. The port needs:

| Libraries | Used for |
| --- | --- |
| kubridge headers and stub library | Loading and patching the Android library |
| OpenSLES | Audio output |
| libsndfile, LAME, mpg123, Opus, Vorbis, Ogg, FLAC | Audio library dependencies |
| vitaShaRK, SceShaccCgExt | Shader compilation |
| taiHEN, mathneon, zlib, pthread | Platform support |

The preflight output gives the exact missing header/archive names. Use [VitaSDK's package manager](https://github.com/vitasdk/vdpm) for available packages; libraries not provided by your SDK must be installed from their upstream source with the same SDK. A normal desktop version of a library cannot replace a Vita library.

The modified vitaGL and other source libraries required by the port are included and built automatically. Do not replace the included vitaGL with a different version to get past an error.

## 3. Build

For a new checkout:

```sh
bash build.sh
```

To rebuild from scratch in an existing checkout, first make sure your terminal has no experimental build overrides. A terminal opened just for this build is a good choice. You can list overrides with:

```sh
env | sort | sed -n '/^GANGSTAR_/p'
```

If that prints options you set for an earlier experiment, unset them or use a fresh terminal. Move the old generated build directory out of the way, then build:

```sh
if [ -d work/build ]; then
    mv work/build "work/build-before-clean-$(date +%Y%m%d-%H%M%S)"
fi
bash build.sh
```

This keeps the old build as a backup. It does not touch your game files or Vita.

Wait for the final `Built target gangstar_rio_wip.vpk-vpk` message. Your package is:

```text
work/build/gangstar_rio_wip.vpk
```

Now follow [Install the game](INSTALLING.md).

## Which settings does this build use?

`build.sh` loads `config/tested.sh`. With no environment overrides, it selects the preferred 32 MB / 32 KB cache configuration, working audio changes and texture optimizations. The cache may fall back to a smaller allocation if the Vita cannot provide enough memory. World textures use smaller existing mip levels from the original OBBs. Streamed lightmaps keep their original layout; four frequently used lightmap sources are cached unchanged in RAM to reduce repeated storage reads. Source caching shares a 64 MiB limit and falls back to original file reads when it cannot allocate memory.

Use `bash build.sh` for this configuration, rather than configuring CMake by hand. Some options are needed together. There is no need to copy files from a developer's `work/` directory, generate texture caches on the computer, or run a separate asset conversion step.

The build preflight checks that the LiveArea images have the required dimensions and are non-interlaced, indexed 8-bit PNGs. VitaShell can reject VPKs whose LiveArea artwork is truecolor or uses alpha; convert replacements to indexed 8-bit PNG before building.

## Check the settings yourself

Before building, print the exact options the script will pass to CMake:

```sh
bash build.sh --show-config
```

This does not compile or contact the Vita. With no overrides, it includes:

```text
-DGANGSTAR_ARCHIVE_CACHE_MIB=32
-DGANGSTAR_ARCHIVE_BLOCK_KIB=32
-DGANGSTAR_ETC1_RESERVE_MIPS=ON
```

After CMake configures, the build script checks every requested option against its actual cache value. A mismatch stops the build. It saves the configured options, vitaGL settings and compiler flags in `work/build/build-config.json`, beside the VPK. Keep that file with a build when comparing settings later.

The source tests also check that every declared Gangstar option appears in both the build script and default profile, and that the preview matches the profile. This catches omitted settings, including option names containing digits.

## Optional source tests

```sh
bash tools/test.sh
```

These check portable code and build-profile defaults. They are useful before submitting code changes, but do not replace testing the game on a Vita.

## If the build fails

- **`VITASDK` is missing:** set the two environment variables in step 1.
- **A header or library cannot be found:** rerun `python3 tools/preflight.py` and install the reported Vita dependency.
- **An old build behaves differently:** check for environment overrides and follow the clean-build steps above.
- **You need help:** report the first compiler/linker error and your SDK version. Do not upload game files or the entire `work/` directory.
