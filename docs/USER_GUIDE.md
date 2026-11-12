# mediabox user guide (English)

mediabox plays music (MP3), shows books (PDF) and plays videos (MP4, MKV, AVI) on the R36S clone running goodluckOS.
The screen is 640x480 and everything is controlled with the gamepad.

## 1. Installing

### 1.1 Build the app (once, on GitHub)
You do not need to compile anything on your computer.

1. Create a GitHub account and a new repository (private is fine), for example `mediabox`.
2. Upload the contents of this project: **Add file > Upload files**, drag in `Dockerfile`, the `src` folder,
   `README.md`, `mediabox.puppy` and the `docs` folder. Upload the `.github` folder as well
   (if the web page refuses hidden folders, use **Add file > Create new file**, type `.github/workflows/build.yml`
   as the name and paste the content of `workflow-build.yml.txt`).
3. Open the **Actions** tab, choose **build-mediabox** on the left and press **Run workflow**.
4. Wait for the green check mark. The very first build can take one to two hours; later builds reuse a cache.
5. Open the finished run, scroll to **Artifacts**, download **mediabox-armhf** and unzip it. Inside is the `mediabox` file.

> Updating later: replace `src/mediabox.c` (and `Dockerfile` if it changed) in the repository and run the workflow again.

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
| Left / Right | seek 10 seconds |
| **L / R** | seek 1 minute |
| Up / Down | volume (0-200 %) |
| **X** | fit (black bars) / fill (crops the sides) |
| **B** or Select | stop and go back |

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
| "Unsupported video format" | The video uses HEVC/VP9/AV1. Re-encode it as H.264. |
| No sound in a video | The audio codec may be unsupported (e.g. DTS). Re-encode to AAC. |
| The language is wrong | Press **Y** on the main menu. |
| Firefox warns about the download | The binary is unsigned and rarely downloaded, which triggers the reputation warning. Build it from your own repository (section 1.1) and keep it only from there. |
| Build fails | Open the failed run on the Actions tab and read the last 40 lines of the log. |

## 8. Files the app writes
- `/home/player/.config/mediabox/settings.cfg` - language
- `/home/player/.config/mediabox/progress.tsv` - last page of each PDF

Delete them to reset the app.
