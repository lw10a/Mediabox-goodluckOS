# mediabox

A small media app for goodluckOS, tested on an **R36S clone (GA36-MB, Allwinner A23)**: an MP3 player, a PDF reader and
an MP4 / MKV / AVI video player in one 640x480 controller-driven app.

*(Türkçe özet için aşağıya bakın / Turkish summary below.)*

## Features
- **Music**: MP3 playback with progress bar, seek, previous/next, shuffle and repeat. Keeps playing while you browse.
- **Books**: PDF reader (MuPDF) with zoom, panning and automatic "continue where you left off".
- **Videos**: H.264 / MPEG-4 video with AAC / MP3 / AC3 / Vorbis / Opus audio, seek, volume, fit/fill.
- **Two interface languages**: English (default) and Turkish. Press **Y** on the main menu or pick the *Language* tile.
- Folder browser (no giant flat lists), internal and external SD card supported.
- No extra libraries needed on the console: FFmpeg and MuPDF are linked statically.

## Quick start
> [!NOTE]
> **No need to build Mediabox yourself!**
>
> Precompiled binaries are available on the [**Latest Releases**](https://github.com/lw10a/Mediabox-goodluckOS/releases/latest) page.
>
> Download `mediabox` and `mediabox.puppy` directly from Releases and follow the installation steps above. No Docker or cross-compilation required!


Full instructions: **[docs/USER_GUIDE.md](docs/USER_GUIDE.md)** (English) ·
**[docs/KULLANIM_KILAVUZU.md](docs/KULLANIM_KILAVUZU.md)** (Türkçe).

For developers: **[ARCHITECTURE.md](ARCHITECTURE.md)** explains how the app works inside, and
**[CONTRIBUTING.md](CONTRIBUTING.md)** explains how to build, test and send changes.

## Controls at a glance
| Where | Keys |
|---|---|
| Menus | Up/Down choose, **A** open, **B** back, **L/R** page, **Start** now playing, **Y** language (main menu) |
| Music | **A** pause, Left/Right seek 10 s, **L/R** previous/next, **X** shuffle, **Y** repeat, **Select** stop |
| Video | **A** pause, Left/Right seek 10 s, **L/R** seek 1 min, Up/Down volume, **X** fit/fill, **B** quit |
| PDF | **L/R** page, **L2/R2** 10 pages, **A** zoom, **X** fit page, D-pad pan, **B** back |

## Repository layout
```
src/mediabox.c               the whole application (C, SDL2)
Dockerfile                   builds MuPDF + a minimal static FFmpeg, then mediabox (armhf)
.github/workflows/build.yml  GitHub Actions workflow that runs the Dockerfile
mediabox.puppy               launcher entry for the goodluckOS / kennel menu
docs/USER_GUIDE.md           user guide (English)
docs/KULLANIM_KILAVUZU.md    kullanım kılavuzu (Türkçe)
ARCHITECTURE.md              internal structure: screens, players, threads, data flow
CONTRIBUTING.md              how to build, test and contribute
LICENSE                      GNU AGPL-3.0-or-later
NOTICE.md                    third-party components and licenses
```

## Settings and data on the console
- Language: `/home/player/.config/mediabox/settings.cfg` (`lang=en` or `lang=tr`)
- PDF reading position: `/home/player/.config/mediabox/progress.tsv`

## Licensing
mediabox is licensed under the **GNU Affero General Public License v3.0 or later**; see [LICENSE](LICENSE).
The binary contains **MuPDF (AGPL-3.0)** and **FFmpeg (LGPL-2.1+)**, which is why the AGPL applies. Details and the
list of third-party components are in [NOTICE.md](NOTICE.md).

---

## Türkçe özet
R36S klon konsolu (goodluckOS) için MP3 oynatıcı, PDF okuyucu ve MP4/MKV/AVI video oynatıcı. Arayüz **İngilizce
(varsayılan)** ve **Türkçe**; ana menüde **Y** tuşuyla ya da *Dil* kutusuyla değişir. Kurulum ve tuşlar için
[docs/KULLANIM_KILAVUZU.md](docs/KULLANIM_KILAVUZU.md) dosyasına bakın. Geliştiriciler için
[ARCHITECTURE.md](ARCHITECTURE.md) ve [CONTRIBUTING.md](CONTRIBUTING.md) dosyaları var; lisans AGPL-3.0'dır
([LICENSE](LICENSE)).
