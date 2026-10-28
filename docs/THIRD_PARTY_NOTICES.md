# Third-party code

This project builds on the Android shared-library loader and vendors the following upstream sources at the recorded commits:

| Component | Upstream | Pinned commit | License file |
| --- | --- | --- | --- |
| FalsoJNI | [v-atamanenko/FalsoJNI](https://github.com/v-atamanenko/FalsoJNI) | `083d5a07e025c6dfbb54ac68fd1acf696c23a86c` | `lib/falso_jni/LICENSE` |
| so_util | [Rinnegatamante/so_util](https://github.com/Rinnegatamante/so_util) | `c4732373e33d808cd885dec3c2c75302cc4c739b` | `lib/so_util/LICENSE` |
| vitaGL | [Rinnegatamante/vitaGL](https://github.com/Rinnegatamante/vitaGL) | `9c23758ff17893db63887f95e9a4d9350c986d88` | `lib/vitagl/COPYING` |

The loader template's original license is retained as [`licenses/soloader-boilerplate-LICENSE`](../licenses/soloader-boilerplate-LICENSE). Other vendored directories retain upstream notices where provided. Review each component's license before redistributing a modified binary or source bundle. The root `LICENSE` retains the original loader MIT text and attribution; component-specific notices and licenses remain applicable. It does not relicense third-party code or game data.

No Gangstar Rio game code, APK, OBB, extracted native library, or game asset is included in this source project or the VPK.

Other retained support code:

- `lib/libc_bridge/` and `lib/fios/`: loader support; retain source notices and the loader MIT license.
- `lib/sha1/`: Brad Conte SHA-1 implementation; preserve its original author/disclaimer headers.
- `lib/vitagl/COPYING.LESSER` and `COPYING`: upstream license texts, shipped with the modified source. The table records upstream provenance; local VitaGL modifications are also part of this tree.
- kubridge headers/stubs are required from the SDK; the redundant local SDK-copy directory and compiled archives are excluded from the public inventory.

The LiveArea images are neutral loader placeholders, not extracted Gangstar artwork. Unused upstream vitaGL samples/media are excluded. Preserve applicable notices if replacing placeholders or adding dependencies.
