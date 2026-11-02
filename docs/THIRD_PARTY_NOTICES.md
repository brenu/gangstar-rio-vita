# Third-party notices

This inventory covers the project’s direct bundled dependencies and external
libraries named by [CMakeLists.txt](../CMakeLists.txt). Dependencies embedded
inside those libraries are documented by their upstream notices.
The port's [GPL-3.0-or-later license](../LICENSE) does not replace the licenses of these
components or grant rights to game data or artwork. File-level notices remain
applicable, including where they differ from a component's main license.

## Bundled components

The commits below record the existing upstream provenance; the checked-in
sources also contain local modifications. LZ4 is recorded by its upstream tag.

| Component | Upstream / recorded revision | License and retained notices |
| --- | --- | --- |
| Android shared-library loader / soloader-boilerplate | Loader support and compatibility code in `source/`; original contributors include Andy Nguyen, Rinnegatamante, GrapheneCt, fgsfds and Volodymyr Atamanenko, with additional file-level contributors | MIT; [original loader license](../licenses/soloader-boilerplate-LICENSE) and individual source headers |
| FalsoJNI | [v-atamanenko/FalsoJNI](https://github.com/v-atamanenko/FalsoJNI), `083d5a07e025c6dfbb54ac68fd1acf696c23a86c` | MIT for FalsoJNI code: [LICENSE](../lib/falso_jni/LICENSE). Android-derived portions retain separate Apache-2.0 notices in their source headers. |
| so_util | [Rinnegatamante/so_util](https://github.com/Rinnegatamante/so_util), `c4732373e33d808cd885dec3c2c75302cc4c739b` | MIT; [LICENSE](../lib/so_util/LICENSE), copyright Andy Nguyen |
| vitaGL | [Rinnegatamante/vitaGL](https://github.com/Rinnegatamante/vitaGL), `9c23758ff17893db63887f95e9a4d9350c986d88` | LGPL-3.0-or-later for vitaGL's own code; [COPYING.LESSER](../lib/vitagl/COPYING.LESSER) and [COPYING](../lib/vitagl/COPYING). Embedded components retain their own source notices. |
| LZ4 library, v1.10.0 | [lz4/lz4](https://github.com/lz4/lz4/tree/v1.10.0/lib); unmodified `lz4.c` / `lz4.h` | BSD-2-Clause, copyright Yann Collet; [LICENSE](../lib/lz4/LICENSE), [provenance and build conditions](../lib/lz4/README.port.md) |
| FIOS loader wrapper | Andy Nguyen; [lib/fios/fios.c](../lib/fios/fios.c) and `fios.h` | MIT notice in `fios.c`; original loader license above. This wrapper is separate from Sony's FIOS2 runtime. |
| Libc bridge declarations and stub-generation inputs | [lib/libc_bridge/](../lib/libc_bridge/), retained from the loader support tree | Preserve the loader's notices. This directory supplies declarations/NIDs and generates import stubs; it does not contain Sony's libc implementation. |
| SHA-1 implementation | Brad Conte; [sha1.c](../lib/sha1/sha1.c), [sha1.h](../lib/sha1/sha1.h) | Original author and “as is” disclaimer retained in both files. These files do not contain a separate named license grant; this inventory does not assign them a different license. |

LZ4 is compiled only when `GANGSTAR_LIGHTMAP_PACK` or
`GANGSTAR_LIGHTMAP_BENCH` is enabled. Both are disabled in the default profile.
The optional host-side cache generators use a separately installed LZ4 library;
compressed game inputs are private generated data, not bundled library source.

## External dependencies

VitaSDK provides the toolchain and standard platform interfaces. Separately
installed libraries remain listed individually because they retain their own
licenses. Exact external package revisions are not pinned by this repository;
retain the notices supplied with the versions used for a build.

| Linked dependency | Purpose / upstream | License / notice source |
| --- | --- | --- |
| VitaSDK | [VitaSDK](https://vitasdk.org/): compiler/toolchain, C/C++ runtimes, libm, pthread implementation, platform headers and import stubs | Component-specific licenses supplied with the SDK; see [platform header documentation](https://docs.vitasdk.org/) and the SDK components’ own notices. VitaSDK does not place all of these components under one shared license. |
| `OpenSLES` | Vita OpenSL ES implementation; [frangarcj/opensles](https://github.com/frangarcj/opensles), with Vita forks | Implementation's own source notices; Khronos Group permission notice in the supplied OpenSL ES API headers. No implementation license text is bundled in this repository. |
| `sndfile` | [libsndfile](https://github.com/libsndfile/libsndfile) | LGPL-2.1-or-later; installed `sndfile.h` and upstream `COPYING` |
| `mp3lame` | [LAME](https://lame.sourceforge.io/) | LGPL-2.0-or-later; installed `lame/lame.h` and upstream `COPYING` |
| `mpg123` | [mpg123](https://www.mpg123.de/) | LGPL-2.1; upstream [COPYING](https://github.com/madebr/mpg123/blob/master/COPYING) and individual source notices |
| `opus` | [Opus](https://github.com/xiph/opus) | BSD-style license and additional notices in upstream [COPYING](https://github.com/xiph/opus/blob/main/COPYING) |
| `vorbis`, `vorbisenc`, `vorbisfile` | [libvorbis](https://github.com/xiph/vorbis) | Xiph.Org BSD-style license; upstream `COPYING` |
| `ogg` | [libogg](https://github.com/xiph/ogg) | Xiph.Org BSD-style license; upstream `COPYING` |
| `FLAC` | [libFLAC](https://github.com/xiph/flac) | Library's BSD-style license; upstream [COPYING.Xiph](https://github.com/xiph/flac/blob/master/COPYING.Xiph). This entry concerns the linked library, not FLAC's command-line programs. |
| `vitashark` | [vitaShaRK](https://github.com/Rinnegatamante/vitaShaRK), runtime shader compilation | LGPL-3.0-or-later notice in installed `vitashark.h`; upstream license texts |
| `SceShaccCgExt` | [bythos14/SceShaccCgExt](https://github.com/bythos14/SceShaccCgExt), shader compiler extensions | Upstream source/package notices; no separate license text is bundled here. [VitaSDK package recipe](https://github.com/vitasdk/packages/blob/master/SceShaccCgExt/VITABUILD) records its source. |
| `mathneon` | math-neon; Lachlan Tychsen-Smith | MIT text in installed `math_neon.h` |
| `z` | [zlib](https://zlib.net/) | zlib license in the dependency's source distribution |
| taiHEN | taiHEN import interface (`taihen_stub`) | License/notices supplied with the installed taiHEN headers/stubs and upstream source |
| kubridge | [kubridge](https://github.com/TheOfficialFloW/kubridge) import interface (`kubridge_stub`) | Upstream headers/stub notices; the kernel plugin is installed separately. The redundant local SDK-copy directory is excluded from the public source inventory. |

Runtime prerequisites such as `kubridge.skprx` and Sony's `libshacccg.suprx`
are installed separately and are not bundled with the loader.

## Retained notices and game data

Bundled dependencies retain their upstream license files and source notices.
Other compatibility code in `source/` retains its file-level attribution and
license headers. The port's GPL-3.0-or-later distribution terms do not replace those original notices.

The original Gangstar Rio APK, OBB files and extracted native library are
user-supplied game data. Optional generated caches and LiveArea artwork are
separate from the software licenses listed here.
