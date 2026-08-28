# Third-party components

Red Carpet Goban is licensed under the **GNU General Public License v3** — see
`LICENSE`. This file lists what it is built from and, for bundles that include
one, what is shipped alongside it.

Every licence below is compatible with distributing the whole under GPLv3. The
complete corresponding source for Goban itself is at
<https://github.com/popojan/goban>.

## Libraries linked into the application

| Component | Licence | Purpose |
|---|---|---|
| [RmlUi](https://github.com/mikke89/RmlUi) | MIT | the menus and dialogs |
| [GLFW](https://www.glfw.org/) | zlib/libpng | window, input, OpenGL context |
| [glyphy](https://github.com/behdad/glyphy) | Apache-2.0 | the text drawn on the board |
| [libsgfcplusplus](https://github.com/herzbube/libsgfcplusplus) | Apache-2.0 | reading and writing SGF |
| [spdlog](https://github.com/gabime/spdlog) | MIT | logging |
| [clipp](https://github.com/muellan/clipp) | MIT | command-line parsing |
| [GLM](https://github.com/g-truc/glm) | Happy Bunny or MIT | vector and matrix maths |
| [nlohmann/json](https://github.com/nlohmann/json) | MIT | the configuration files |
| [glad](https://glad.dav1d.de/) | MIT | OpenGL loader (vendored in `src/glad/`) |
| [FreeType](https://freetype.org/) | FTL or GPLv2 | font rasterisation |
| [PortAudio](https://www.portaudio.com/) | MIT | sound output |
| [libsndfile](https://libsndfile.github.io/libsndfile/) | LGPL-2.1-or-later | reading the stone sounds |

On Windows these are linked statically from vcpkg; on Linux and macOS FreeType,
PortAudio and libsndfile come from the system. libsndfile is LGPL, which permits
static linking provided the recipient can relink — satisfied here because the
whole application is GPLv3 and its source is public.

[doctest](https://github.com/doctest/doctest) (MIT) is used by the test suite and
is not part of any shipped binary.

## Fonts

| Font | Licence |
|---|---|
| Noto Sans, Noto Sans CJK SC | SIL Open Font License 1.1 |
| Roboto (`config/fonts/default-font.ttf`) | Apache-2.0 |

## Go engines

Goban plays through external GTP engines and contains none of its own.

Bundles distributed from [hraj.si/goban](https://hraj.si/goban) include **GNU Go
3.8** for convenience. GNU Go is copyright the Free Software Foundation and
licensed under the **GNU General Public License v3**. Bundles published on GitHub
Releases contain no engine at all.

The corresponding source for those binaries is the official tarball

    https://ftp.gnu.org/gnu/gnugo/gnugo-3.8.tar.gz
    sha256 da68d7a65f44dcf6ce6e4e630b6f6dd9897249d34425920bfdd4e07ff1866a72

**plus one patch**, `deps/_patches/gnugo-3.8-implicit-int.patch` in this
repository. Two declarations of `verifyW32()` use an implicit `int` return type,
which C99 removed and current mingw-w64 rejects, so 3.8 cannot be cross-compiled
for Windows without it. That patch is the whole of the difference: the build tree
was diffed against the tarball on 2026-08-28 and 2256 files were identical, one
differed, and nothing was added or removed.

KataGo, Pachi and others are configured in `config/base.json` but never shipped;
each carries its own licence from its own project.
