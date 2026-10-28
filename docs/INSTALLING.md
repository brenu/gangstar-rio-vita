# Install the game

You need a homebrew-enabled PS Vita, VitaShell, the port's VPK, and your own supported Android game files. Allow about **2.2 GB** for the original files, plus extra free space for the VPK, saves and generated caches.

## 1. Check the Vita requirements

Install the [kubridge kernel plugin](https://github.com/TheOfficialFloW/kubridge) using its upstream instructions. If you already have it working for this port, keep that installation.

You also need `libshacccg.suprx`, the Vita shader compiler. Follow the [vitaGL prerequisites](https://github.com/Rinnegatamante/vitaGL#prerequisites) if it is not already installed.

VitaCompanion is not needed for manual installation or playing the game.

## 2. Prepare your Android files

The supported version is **Gangstar Rio: City of Saints v1.2.1, ARMv7**, with these two original OBBs:

```text
main.12030.com.gameloft.android.ANMP.GloftG4HM.obb
patch.12136.com.gameloft.android.ANMP.GloftG4HM.obb
```

Keep the names exactly as shown. Do not extract or modify the OBBs. Other game revisions may not work.

There are two ways to prepare the files. Choose either one.

### Option A: use the preparation script

On your computer, arrange your files like this, outside the repository:

```text
my-game-files/
├── Gangstar Rio City of Saints v1.2.1.apk
└── com.gameloft.android.ANMP.GloftG4HM/
    ├── main.12030.com.gameloft.android.ANMP.GloftG4HM.obb
    └── patch.12136.com.gameloft.android.ANMP.GloftG4HM.obb
```

From the repository directory, run the following, replacing `/path/to/my-game-files` with your actual folder:

```sh
python3 tools/stage_data.py --input-dir "/path/to/my-game-files" --dry-run
python3 tools/stage_data.py --input-dir "/path/to/my-game-files"
```

The first command verifies the files without copying them. The second prepares the four files in:

```text
work/staging/ux0/data/gangstarrio/
```

The script checks file hashes to catch a wrong revision or damaged input. If it reports `Revision mismatch`, do not bypass the check or rename another revision to match. It only prepares files on your computer; it does not upload anything to the Vita.

### Option B: prepare the files manually

Open your APK with a ZIP archive manager and extract these two files:

```text
lib/armeabi-v7a/libgl2jni.so
lib/armeabi-v7a/libgenerator.so
```

Use the `armeabi-v7a` libraries, not libraries for another CPU. Put them beside the two original OBBs. You do not need to copy the APK itself to the Vita.

For reference, the supported files have these sizes:

| File | Size in bytes |
| --- | ---: |
| `libgl2jni.so` | 15,335,236 |
| `libgenerator.so` | 447,860 |
| `main.12030.com.gameloft.android.ANMP.GloftG4HM.obb` | 2,119,380,688 |
| `patch.12136.com.gameloft.android.ANMP.GloftG4HM.obb` | 3,709,754 |

Matching sizes alone do not confirm the revision; the preparation script verifies hashes as well.

## 3. Copy the four files to the Vita

Use VitaShell's USB or FTP mode. The final layout must be:

```text
ux0:/data/gangstarrio/
├── libgl2jni.so
├── libgenerator.so
├── main.12030.com.gameloft.android.ANMP.GloftG4HM.obb
└── patch.12136.com.gameloft.android.ANMP.GloftG4HM.obb
```

Copy the **contents** of the prepared `gangstarrio` folder into that directory. Avoid accidentally creating `gangstarrio/gangstarrio/`. When using USB, `ux0:` is the root of the mounted storage, so the folder on your computer's mounted drive is `data/gangstarrio/`.

## 4. Install the VPK

Copy `work/build/gangstar_rio_wip.vpk` from your computer to the Vita, for example to `ux0:/data/`. In VitaShell, select the VPK, press X and follow the installation prompts.

The application title is **Gangstar Rio - WIP** and its title ID is `G4HM00001`. Once installation finishes, launch its bubble from the home screen.

## 5. First launch

The first load can take a while. Leave the game running while it prepares its caches, then touch the screen when prompted. You should reach the menu and be able to start or resume a game. Cached data is generated from your original OBBs; you do not need someone else's texture cache or shader files.

Sound should play normally. Left stick moves the character and steers; right stick controls the camera. Hold Cross to sprint on foot; Triangle handles nearby shop/item interactions or vehicle entry/exit, and Square attacks. R accelerates and L brakes in a car. See the [full control layout](../README.md#controls) for cover, weapons, consumables, map and pause. Menus and some contextual prompts still use the touchscreen.

Streaming stutters are still a known issue. Please report a crash, missing sound or incorrect graphics rather than treating them as expected first-launch behavior.

## Updating or testing a fresh install

For a normal update, close the game and install the new VPK. Keep `ux0:/data/gangstarrio/` and your saves.

Deleting the bubble does not necessarily remove the separate data folder. Before uninstalling or moving that folder, back it up if you want to keep your progress. For a clean-install test, move the existing `gangstarrio` folder somewhere safe, install the VPK, and recreate the four-file layout in step 3. That avoids carrying over old caches without deleting your backup. No files from a developer's `work/` directory are needed.

## Troubleshooting

- **Missing game files:** check the exact folder, filenames and file sizes above. Make sure the transfer completed.
- **It exits before the menu:** check kubridge and the supported game revision. The port checks native code signatures and cannot accept arbitrary APK versions.
- **Shader/compiler error:** check that `libshacccg.suprx` is installed.
- **A new source build feels slower:** build with the default profile and no experimental environment overrides, as described in [the build guide](BUILDING.md).

The game writes `ux0:/data/gangstarrio/port.log`. If you need help, share the relevant error lines and what happened on screen. Do not upload your APK, OBBs, original libraries, saves or full data folder.
