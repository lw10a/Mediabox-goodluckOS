# Third-party components

mediabox itself is only the code in `src/mediabox.c`. The binary built by the Dockerfile links these components:

| Component | Version | License | Linking |
|---|---|---|---|
| [MuPDF](https://mupdf.com) | 1.24.9 | AGPL-3.0 | static |
| [FFmpeg](https://ffmpeg.org) (libavformat, libavcodec, libswscale, libswresample, libavutil) | n6.1.1, minimal build, no GPL or non-free parts | LGPL-2.1-or-later | static |
| [dr_mp3](https://github.com/mackron/dr_libs) | latest at build time | public domain / MIT-0 | header only |
| [SDL2](https://libsdl.org), SDL2_ttf | from the console | zlib | dynamic (already on the console) |
| Inter font | already on the console | SIL OFL 1.1 | loaded at run time |

## What this means for you
- mediabox's own code is licensed under the **GNU Affero General Public License v3.0 or later** (see `LICENSE`).
  The reason: MuPDF is AGPL-3.0 and is linked statically into the binary, so the distributed binary and its
  corresponding source (this repository) must be offered under AGPL-3.0-compatible terms. The other option would be a
  commercial license for MuPDF from Artifex, or replacing the PDF engine.
- If you publish the binary (for example as a GitHub Release), keep the source available and keep this notice.
- FFmpeg is built without `--enable-gpl` and `--enable-nonfree`, so it stays LGPL. Its source is fetched from the
  upstream tag named in the `Dockerfile`.
- For private use on your own console nothing needs to be published.
