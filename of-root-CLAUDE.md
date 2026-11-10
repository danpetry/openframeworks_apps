# CLAUDE.md

This file provides guidance to Claude Code (claude.ai/code) when working with code in this repository.

## Repository structure

This directory is a full **openFrameworks 0.12.1** distribution for Visual Studio / 64-bit (downloaded, not a git repo itself). The framework internals (`libs/`, `scripts/`, `examples/`, `addons/`) are vendored code — do not modify them unless specifically asked to patch the framework.

**The actual project work lives under `apps/myApps/`**, which is its own git repository (remote: `github.com/danpetry/openframeworks_apps`). Almost all requests will concern files in there. Each subdirectory of `apps/myApps/` is an independent openFrameworks project (its own `.sln`/`.vcxproj`, `src/`, `bin/`, `addons.make`, `config.make`), typically named by date and topic, e.g.:

- `2025_11_26_musicclip/` — current/most active sketch
- `2025_10_17_musicclip/` — earlier iteration
- `Template_1_reactive_bounce/` — reusable starting template for audio-reactive sketches
- `mySketch/`, `emptyExample/` — scratch/starter sketches

New sketches are generally created by copying an existing project folder (e.g. `Template_1_reactive_bounce`) and renaming it, then updating the `.sln`/`.vcxproj` project name and GUID. When doing this, check for accidental stray copies (past commits have left duplicate `.sln`/`.vcxproj` files with the old template name, or stray folders like `cpp src/` sitting next to `src/`) — clean these up rather than propagating them into new sketches.

## Build & run

Use `apps/myApps/build.ps1` to build (and optionally launch) any sketch from the command line, without opening Visual Studio. It wraps MSBuild (located via `vswhere`, so no hardcoded VS path) and targets the `Debug|x64` / `Release|x64` configurations defined in each `.sln`.

```powershell
cd apps/myApps
.\build.ps1                                              # list available project folders
.\build.ps1 -Project 2025_11_26_musicclip                 # Debug build
.\build.ps1 -Project 2025_11_26_musicclip -Configuration Release
.\build.ps1 -Project 2025_11_26_musicclip -Target clean    # or rebuild
.\build.ps1 -Project 2025_11_26_musicclip -Run             # build then launch the .exe (detached)
```

Notes:
- The script picks the `.sln` whose filename matches the project folder, to guard against stray leftover `.sln`/`.vcxproj` files from copy-pasted templates (see below); it warns if it has to fall back to a different one.
- Built executables land in `<project>/bin/<ProjectName>_debug.exe` (Debug) or `<project>/bin/<ProjectName>.exe` (Release).
- `-Run` has real side effects: these sketches open a live audio stream and several start MP4 recording unconditionally in `setup()`, writing to `bin/data/` — don't use `-Run` as a routine build-verification step without expecting it to record.
- Manual alternative: open the `.sln` in Visual Studio and hit F5 (right-click the project → "Set as StartUp Project" if it complains). Runtime data files (`.wav`/`.mp4` assets referenced via `ofToDataPath(...)`) must exist under `bin/data/` for a sketch to run correctly either way.

There are no automated tests in `apps/myApps`; verification is a successful CLI/VS build plus manually running the app and observing the rendered/recorded output.

## Architecture of an oF sketch

Every project follows the standard openFrameworks pattern:

- `src/main.cpp` — creates the window and launches `ofApp`.
- `src/ofApp.h` / `src/ofApp.cpp` — the app class implementing `ofBaseApp`: `setup()` runs once, `update()` runs every frame before `draw()`, plus input callbacks (`keyPressed`, `mouseMoved`, etc.).
- `addons.make` — lists the openFrameworks addons the project links against (one per line); `config.make` is normally empty (default build settings).

Common pattern across the music-visualizer sketches in `apps/myApps`:

- **Audio pipeline**: `ofSoundStream` → `ofxSoundPlayerObject` (file playback) → `FftObject` (custom `ofxSoundObject` subclass wrapping `ofxFft`, in `src/FftObject.h/.cpp`, shared/copied across sketches) → `ofxSoundOutput`. `update()` samples FFT amplitude per band (often on a log-frequency axis) into a `bands` vector that `draw()` visualizes.
- **Rendering**: drawing happens into an offscreen `ofFbo` sized for vertical/portrait video (e.g. 1080x1920 for Reels/Shorts), often with a "ping-pong" pair of FBOs (`fboOutput` / `lastFboOutput`, swapped each frame) to create feedback/trail effects. The FBO is then scaled and blitted to the visible window.
- **Recording**: `ofxFastFboReader` reads the FBO to `ofPixels` efficiently, then `ofxFFmpegRecorder` encodes it to `.mp4` (`libx264`, CRF-based quality). Recording is typically started unconditionally in `setup()` rather than toggled via keypress — check whether recording is active before assuming interactive-only behavior.

Key addons in use (see `addons/<name>` for source): `ofxAudioFile`, `ofxFft`, `ofxSoundObjects`, `ofxFastFboReader`, `ofxFFmpegRecorder`, `ofxGui`.

## Creating new projects

New oF projects can be scaffolded with `projectGenerator/projectGenerator.exe` (GUI tool) instead of hand-copying folders, when adding addons or generating clean project files is needed.
