# Contributing to mediabox

Thanks for your interest! Bug reports, device test results, translations and code changes are all welcome.
Please read [ARCHITECTURE.md](ARCHITECTURE.md) first if you want to change code.

## Ways to help

- **Report a bug** or a device that does not work. Open an issue with:
  - device and OS (for example "R36S clone GA36-MB, goodluckOS"), and the mediabox version or commit;
  - what you did, what you expected and what happened;
  - for video problems: container, codec, resolution, bitrate and the **Dropped** number shown in the overlay;
  - for build problems: the last 40 lines of the failed Actions log.
- **Test on other hardware** and tell us what works. Right now only the R36S clone has been tested.
- **Translate the interface** (section 4) or **improve the guides**.
- **Fix bugs or add features** with a pull request (section 3).

## 1. Building

You do not need a development machine; GitHub builds the binary.

### Option A: GitHub Actions in your fork (recommended)
1. Fork the repository.
2. **Actions** tab: click **I understand my workflows, go ahead and enable them**.
3. **build-mediabox > Run workflow**. Download **Artifacts > mediabox-armhf**.

The first build in a fork takes one to two hours (MuPDF and FFmpeg are compiled under ARM emulation) because caches
are not shared between repositories. Later builds in the same fork reuse the cache.

### Option B: the same build locally with Docker
```bash
docker run --privileged --rm tonistiigi/binfmt --install arm       # once: ARM emulation
docker buildx build --platform linux/arm/v7 --target out --output type=local,dest=dist .
# result: dist/mediabox
```
This is the same Dockerfile that CI uses. Expect a long first run and fast reruns when only `src/mediabox.c` changes.

### Option C: on the device or another ARM machine
Install `build-essential`, `libsdl2-dev`, `libsdl2-ttf-dev`, build MuPDF 1.24.9 and FFmpeg n6.1.1 as in the
`Dockerfile`, then compile `src/mediabox.c` with the same `gcc` command. Not needed for normal contributions.

### Quick check before you push
Compile errors are the most common reason for a failed run. If you have the dependencies installed you can run
`gcc -fsyntax-only -std=gnu11 -Wall -Wextra src/mediabox.c` with the right `-I` paths; otherwise let Actions do it.

## 2. Testing on a device
There are no automated tests; please check by hand and say what you tested in the pull request:

- [ ] App starts, main menu shows five tiles, **Y** switches the language, the choice survives a restart
- [ ] Music: play, pause, seek, next/previous, shuffle, repeat, background playback while browsing
- [ ] Books: open a PDF, change zoom, pan, close and reopen (page is remembered)
- [ ] Videos: play an H.264/AAC mp4, pause, seek (10 s and 1 min), volume, fit/fill, end of file returns to the list
- [ ] Videos: check sound and picture stay in sync for a few minutes; note the **Dropped** counter
- [ ] Empty folder, missing folder and an unsupported file show a sensible message
- [ ] Both languages checked if you touched any text

## 3. Code changes

### Workflow
1. Open an issue for larger changes so we can agree on the approach first.
2. Create a branch in your fork, make small focused commits with clear messages (what and why).
3. Open a pull request that describes the change, the device you tested on and the checklist results.
4. Update `ARCHITECTURE.md` and both user guides (`docs/USER_GUIDE.md` and `docs/KULLANIM_KILAVUZU.md`) when behavior
   or controls change. The two guides should always say the same thing.

### Code style
- **C11 (`-std=gnu11`)**, one file, 4-space indentation, no tabs, no trailing whitespace.
- Keep the banner-comment sections and the order described in ARCHITECTURE.md.
- Prefer `static` functions and small files-scope state; prefix video globals with `v_`.
- Keep the code warning-free with `-Wall`.
- Write new comments in English. Existing Turkish comments may stay.
- Do not add dependencies lightly: everything must link statically or already exist on the console.
- Performance matters: avoid per-frame allocation, use the text cache (`draw_text`) and the `dirty` flag, and never block
  the main thread.
- Threads: shared video state is protected by `v_mx` (see ARCHITECTURE.md section 7). Do not touch it without the mutex.

### Video and codecs
Extra codecs mean extra FFmpeg configure flags (`--enable-decoder=...`, `--enable-parser=...`, `--enable-demuxer=...`).
Keep the build free of GPL and non-free parts (do not add `--enable-gpl` or `--enable-nonfree`), and mention the size
and speed impact in the pull request.

## 4. Translations and text

All user-visible strings are in the `STR` table at the top of `src/mediabox.c`, indexed by the `X_*` enum.

**Changing or adding a message**
1. Add an `X_NAME` id to the enum.
2. Add the row `[X_NAME] = {"English text", "Türkçe metin"},` to `STR`.
3. Use it with `TX(X_NAME)`.

**Adding a whole new language** is a small code change: add it to `Lang`, add a column to `STR` (every row), update
`TX`, `settings_load`/`settings_save` and make `lang_toggle` and the home language tile cycle through the languages.
The font must contain the language's characters (the Inter font used on the console covers Latin, Greek and Cyrillic only).

## 5. Documentation
- English is the default language of the repository. Keep `docs/USER_GUIDE.md` and `docs/KULLANIM_KILAVUZU.md` in sync.
- Screenshots go to `docs/images/` and are referenced with relative paths.

## 6. License of contributions
mediabox is licensed under the **GNU Affero General Public License v3.0 or later** (see [LICENSE](LICENSE)); the reasons
are in [NOTICE.md](NOTICE.md). By submitting a contribution you agree that it is licensed under the same terms
(inbound = outbound). Do not copy code from sources with incompatible licenses.

## 7. Be kind
Be respectful and patient in issues and pull requests. This is a hobby project for a small device, and beginners are
welcome.
