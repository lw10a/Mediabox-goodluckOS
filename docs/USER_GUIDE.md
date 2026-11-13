# mediabox user guide (English)

mediabox plays music (MP3), shows books (PDF) and plays videos (MP4, MKV, AVI) on a handheld with a 640x480 screen and a
gamepad. It was written for and tested on an R36S clone (GA36-MB, Allwinner A23) running goodluckOS; other 32-bit ARM
Linux devices with SDL2 and SDL2_ttf may work but are untested (see the Compatibility table in `README.md`).

## 1. Installing

### 1.1 Build the app (once, on GitHub)
You do not need to compile anything on your computer. GitHub builds the app for you.

**Option A - fork the repository (easiest)**
1. Sign in to GitHub and click **Fork** at the top right of the mediabox repository. You get your own copy.
2. Open the **Actions** tab of your copy. GitHub switches workflows off in forks, so click
   **I understand my workflows, go ahead and enable them**.
3. Choose **build-mediabox** on the left and press **Run workflow** (twice: once to open the box, once to confirm).
4. Wait for the green check mark. The very first build can take one to two hours; later builds reuse a cache.
5. Open the finished run, scroll to **Artifacts**, download **mediabox-armhf** and unzip it. Inside is the `mediabox` file.

**Option B - upload the files to a new repository**
1. Create a new repository (private is fine), for example `mediabox`.
2. **Add file > Upload files**: drag in `Dockerfile`, the `src` folder, `README.md`, `NOTICE.md`, `mediabox.puppy` and
   the `docs` folder. Upload the `.github` folder as well. If the web page refuses hidden folders, use
   **Add file > Create new file**, type `.github/workflows/build.yml` as the name and paste the content of
   `workflow-build.yml.txt`.
3. Continue with steps 3-5 of option A.

> Private repositories have a monthly limit of free Actions minutes; a first build can use a large part of it.
> Public repositories are not limited.
>
> Updating later: change `src/mediabox.c` (and `Dockerfile` if needed) in your repository, or sync your fork with the
> original, and run the workflow again.

### 1.2 Copy it to the console
1. Create the folder `/home/player/ports/mediabox/` and put the `mediabox` file inside.
   If needed: `chmod +x /home/player/ports/mediabox/mediabox`
2. Copy `mediabox.puppy` to `/home/player/.local/share/applications/` (next to `kennel.puppy`).
3. Reboot or reopen the menu. In the **Media** category you will see the app.

### 1.3 Add your files
| Content | Internal card | External card | Types |
|---|---|---|---|
| Music | `/home/player/music` | `/media/external/music` | `.mp3` |
| Books | `/home/player/books` | `/media/external/books` | `.pdf` |
| Videos | `/home/player/videos` | `/media/external/videos` | `.mp4 .m4v .mov .mkv .avi` |

Sub-folders are fine. Video files that are inside the music folder are also listed under **Music** and open as videos.
Dot-files (names starting with `.`) are hidden.

## 2. Main menu
Five tiles: **Music**, **Books (PDF)**, **Videos**, **Language** and **Help**.

| Key | Action |
|---|---|
| Up / Down | choose a tile |
| **A** | open the tile |
| **Y** | switch language (English / Turkish) |
| **Start** | jump to the "Now playing" screen (when music is loaded) |
| **B** | quit the app |

The language is saved and used the next time too. English is the default.

## 3. Browsing folders
| Key | Action |
|---|---|
| Up / Down | move |
| **A** | open folder / play file |
| **B** | go up one folder (back to the main menu at the top) |
| **L / R** | jump a page up / down |
| **Start** | open "Now playing" |

If both cards have the folder, you first choose **Internal card** or **External card**.
A bar at the bottom shows the music that is currently playing, so you can keep listening while you read a book or browse.

## 4. Music player
| Key | Action |
|---|---|
| **A** or Start | pause / resume |
| Left / Right | seek back / forward 10 seconds |
| **L** | restart the track (or the previous one if it just started) |
| **R** | next track |
| **X** | shuffle on/off |
| **Y** | repeat: off, whole folder, one track |
| **Select** | stop and clear the playlist |
| **B** | back to the list (music keeps playing) |

Opening a file queues all MP3 files of that folder; the next track starts automatically.

## 5. Video player
| Key | Action |
|---|---|
| **A** or Start | pause / resume |
| Left / Right | seek 10 s. **Hold to speed up**: the step grows to 20, 40 and then 90 s |
| **L / R** | seek 1 minute per press (several presses add up) |
| Up / Down | volume, 5 % steps (this is the system volume, see below) |
| **X** | fit (black bars) / fill (crops the sides) |
| **Y** | right after opening, while "Continuing from..." is shown: start from the beginning |
| **B** or Select | stop and go back (the position is saved) |

**Seeking.** While you hold Left/Right the picture freezes and the control bar shows the target time and the amount
(for example `Seek +2:40`). The jump happens once, when you let go (or press **A** to jump immediately).

**Resume (videos only).** mediabox remembers where you stopped in each video. Next time it continues from there and
shows "Continuing from ..." for 8 seconds; press **Y** to start over instead. The position is saved every 15 seconds and
when you leave the video, and it is cleared when you watch to the end (the last 30 seconds count as the end).
In the video list the saved time is shown at the right of the file name. The last 128 videos are remembered.

**Volume.** Up/Down changes the console's real (ALSA) volume, the same one the side volume buttons change, so the number
in the control bar always matches. If the sound system cannot be reached, the app falls back to its own volume (0-200 %).
If the number does not follow the side buttons, tell the app which mixer control the buttons use by adding a line
`mixer=NAME` to `settings.cfg` (list the names with `amixer scontrols`, for example `mixer=Headphone`).

The control bar appears for a few seconds after every key press and while paused. It shows the title, a progress bar,
the time, the volume, the fit mode and **Dropped**, the number of frames skipped because the CPU was too slow.

### Making videos play smoothly
The console decodes video with its CPU only (no hardware decoder). Convert big or high-bitrate videos on your computer first.
With [FFmpeg](https://ffmpeg.org):

```
ffmpeg -i input.mkv -vf "scale=-2:360" -c:v libx264 -profile:v main -preset slow -crf 25 -r 30 -c:a aac -b:a 128k -ac 2 output.mp4
```

Good targets: **360p to 480p**, H.264 Baseline or Main, 24-30 fps, around 800-1200 kbps video, AAC stereo audio.
Not supported: HEVC/H.265, VP9, AV1. If **Dropped** grows fast, lower the resolution or bitrate.

## 6. PDF reader
| Key | Action |
|---|---|
| **L / R** | previous / next page |
| **L2 / R2** | 10 pages back / forward |
| **A** | zoom: fit page, fit width, 150 %, 200 %, 300 % |
| **X** | back to "fit page" |
| D-pad / left stick | scroll a zoomed page (at the edge it flips the page) |
| **B** | close (the page you were on is remembered) |

Password-protected PDFs cannot be opened. Very large pages are rendered at a limited resolution to save memory.

## 7. Troubleshooting
| Problem | What to do |
|---|---|
| "Folder not found" | Create the folder from section 1.3 (names are lower case). |
| Video stutters | Convert it to 360p-480p (section 5). Check the **Dropped** counter. |
| Volume in the app differs from the side buttons | Add `mixer=NAME` to `settings.cfg` (section 5, Volume). |
| "Unsupported video format" | The video uses HEVC/VP9/AV1. Re-encode it as H.264. |
| No sound in a video | The audio codec may be unsupported (e.g. DTS). Re-encode to AAC. |
| The language is wrong | Press **Y** on the main menu. |
| Firefox warns about the download | The binary is unsigned and rarely downloaded, which triggers the reputation warning. Build it from your own repository (section 1.1) and keep it only from there. |
| Build fails | Open the failed run on the Actions tab and read the last 40 lines of the log. |

## 8. Files the app writes
- `/home/player/.config/mediabox/settings.cfg` - language (`lang=en|tr`) and the optional `mixer=NAME`
- `/home/player/.config/mediabox/progress.tsv` - last page of each PDF
- `/home/player/.config/mediabox/video_progress.tsv` - resume position of each video

Delete them to reset the app.
