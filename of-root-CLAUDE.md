# CLAUDE.md

This file provides guidance to Claude Code (claude.ai/code) when working with code in this repository.

(The canonical copy lives in the myApps repo as `apps/myApps/of-root-CLAUDE.md`, since the oF root isn't version-controlled; copy it to the oF root as `CLAUDE.md`. Edit the repo copy and re-copy.)

## Repository structure

This directory is a full **openFrameworks 0.12.1** distribution (downloaded, not a git repo itself). The primary machine is now **macOS (Apple Silicon)**; the project previously lived on Windows / Visual Studio x64, and the sketches still carry their `.sln`/`.vcxproj` files and `build.ps1`. The framework internals (`libs/`, `scripts/`, `examples/`) are vendored code; don't modify them unless specifically asked to patch the framework.

Third-party addons in `addons/` are separate git clones, pinned to specific commits (see `apps/myApps/MAC_MIGRATION.md` for remotes/commits): `ofxAudioFile`, `ofxFft`, `ofxSoundObjects`, `ofxFastFboReader`, and `ofxFFmpegRecorder` (Dan's fork, branch `dpetry/handle-paths-with-spaces`). Everything else in `addons/` is stock oF.

**The actual project work lives under `apps/myApps/`**, which is its own git repository (remote: `github.com/danpetry/openframeworks_apps`). Almost all requests will concern files in there. Each subdirectory of `apps/myApps/` is an independent openFrameworks project (`src/`, `bin/`, `addons.make`, `config.make`, plus build files), typically named by date and topic:

- `Template_1_reactive_bounce/` — the maintained starting template for audio-reactive sketches. It's the only sketch ported to macOS (has a `Makefile`).
- `2025_*` / `2026_*` dated folders: legacy sketches that have already been rendered and posted. They are Windows-only and intentionally left as-is; don't port or "fix" them unless asked.
- `emptyExample/`: stock starter.

New sketches are created by copying `Template_1_reactive_bounce` and renaming the folder (on Mac the Makefile picks up the folder name; the Windows `.sln`/`.vcxproj` would need renaming too). Check for accidental stray copies (old template-named `.sln`/`.vcxproj`, stray folders like `cpp src/` next to `src/`) and clean them up rather than propagating them.

## Build & run (macOS)

Builds use the oF Makefiles with only the Xcode command-line tools; full Xcode isn't installed or needed. `apps/myApps/build.sh` wraps them:

```bash
cd apps/myApps
./build.sh                                             # list projects that have a Makefile
./build.sh Template_1_reactive_bounce                  # Debug build
./build.sh Template_1_reactive_bounce Release
./build.sh Template_1_reactive_bounce Debug clean      # or rebuild
./build.sh Template_1_reactive_bounce --run            # build then launch (runs in the foreground)
```

Notes:
- First build of any project also compiles the oF core library (a few minutes); later builds are incremental.
- Output: `<project>/bin/<Project>_debug.app` (Debug) or `<project>/bin/<Project>.app` (Release). `bin/data/` is where `ofToDataPath` resolves; `bin/` is gitignored, so audio assets and recordings are not in git.
- To port another sketch: copy `scripts/templates/osx/Makefile` into it (`config.make`/`addons.make` already exist), then apply the template's `#ifdef TARGET_WIN32` / `TARGET_OSX` audio-API and ffmpeg-path guards in `setup()`.
- Running has real side effects: the sketch opens an audio stream and, if `bRecordEnabled` is true, writes a timestamped `.mp4` to `bin/data/`. ESC calls `recorder.stop()` and exits cleanly; killing the process closes ffmpeg's stdin, which also finalizes the file.
- ffmpeg comes from Homebrew (`/opt/homebrew/bin/ffmpeg`), set explicitly via `recorder.setFFmpegPath` because GUI-launched apps don't get the shell PATH.
- VS Code: open the sketch folder itself (not myApps). `.vscode/tasks.json` has `osx` overrides calling `build.sh`; launch config "Build and launch (Debug, macOS)" uses cpptools' lldb. `c_cpp_properties.json` has a `Mac` configuration.

There are no automated tests; verification is a successful build plus running the app. Useful headless checks: the periodic `bands:` log line (min/max per band should move when audio plays), and `ffprobe` on the recorded `.mp4` (expect h264, 1080x1920, yuv420p, 30fps).

Windows (legacy): `build.ps1` wraps MSBuild for the `.sln` files; see git history for details.

## Architecture of an oF sketch

Every project follows the standard openFrameworks pattern:

- `src/main.cpp` — creates the window and launches `ofApp`.
- `src/ofApp.h` / `src/ofApp.cpp` — the app class implementing `ofBaseApp`: `setup()` runs once, `update()` runs every frame before `draw()`, plus input callbacks (`keyPressed`, `mouseMoved`, etc.).
- `addons.make` — lists the openFrameworks addons the project links against (one per line); `config.make` is normally empty (default build settings).

Common pattern across the music-visualizer sketches in `apps/myApps`:

- **Audio pipeline**: `ofSoundStream` → `ofxSoundPlayerObject` (file playback) → `FftObject` (custom `ofxSoundObject` subclass wrapping `ofxFft`'s KISS FFT backend, in `src/FftObject.h/.cpp`) → `ofxSoundOutput`. `update()` samples FFT amplitude on a log-frequency axis into a `bands` vector (with per-band attack/release smoothing) that `draw()` visualizes. This is cross-platform (RtAudio → CoreAudio on Mac). Don't replace it with `ofSoundGetSpectrum()`, which only sees `ofSoundPlayer`/FMOD audio.
- **Rendering**: drawing happens into an offscreen `ofFbo` sized for vertical/portrait video (1080x1920 for Reels/Shorts), with a "ping-pong" pair of FBOs (`fboOutput` / `lastFboOutput`) for feedback/trail effects. The FBO is then scaled and blitted to the visible window.
- **Recording**: `ofxFastFboReader` reads the FBO to `ofPixels`, then `ofxFFmpegRecorder` pipes raw frames to ffmpeg (`libx264`, CRF 18). Controlled by `bRecordEnabled` (template default: off); recording starts on the first frame real audio has been processed.

## Creating new projects

New oF projects can also be scaffolded with `projectGenerator/projectGenerator.app` (GUI) when generating clean project files (e.g. an `.xcodeproj`) is needed.
