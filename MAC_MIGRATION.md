# Moving this setup from Windows to macOS

Context captured on the Windows machine (2026-11-10) so the Mac can be set up
quickly. Written for both Dan and Claude Code on the new machine: read this
first, then work through the checklist.

## Outcome on the Mac (2026-11-12)

Done and verified on macOS 27 / Apple Silicon. Where reality differed from the plan below:

- **No Xcode needed.** Only the command-line tools are installed; the oF `Makefile`
  (`scripts/templates/osx/Makefile`, copied into the sketch) builds fine with them.
  `projectGenerator` wasn't used.
- **Only `Template_1_reactive_bounce` was ported.** The dated sketches are finished/posted
  and stay Windows-only on purpose.
- Addons cloned at the commits in section 2 (over https). The `ofxFFmpegRecorder` fork's
  one commit (quote output path with spaces + log the ffmpeg command) is still relevant on Mac
  since `popen` goes through `/bin/sh` there too, so it's kept.
- Template `setup()` changes: `setApi(MS_WASAPI)` and `numInputChannels = 2` are now
  `#ifdef TARGET_WIN32`; on other platforms the stream opens with 0 input channels (nothing
  reads input, and requesting it would trigger the macOS mic-permission prompt).
  `#ifdef TARGET_OSX` → `recorder.setFFmpegPath("/opt/homebrew/bin/ffmpeg")`.
- Verified by a run with `2026_10_08_tranceysolo.wav`: band stats moving in the log, ffmpeg
  keeping up at ~29fps realtime, output mp4 = h264 1080x1920 yuv420p 30fps, frames render
  bars with trails. (ffmpeg warns "Multiple -pix_fmt options": harmless, the sketch's
  `-pix_fmt yuv420p` wins.)
- `build.sh` added (Mac counterpart of `build.ps1`). VS Code: `tasks.json` got `osx` overrides
  calling it, `launch.json` got a "Build and launch (Debug, macOS)" config using cpptools'
  lldb (`cppdbg`/`MIMode: lldb`, so CodeLLDB isn't needed), `c_cpp_properties.json` got a
  `Mac` config. Windows variants kept. The Windows editor font/theme weren't carried over;
  the Mac's editor setup comes from `~/Sandbox/devconfig`.
- Root `CLAUDE.md` rewritten for Mac (canonical copy: `of-root-CLAUDE.md`).
- Mac-only crash found afterwards: with `bShowFilePicker = true`, the macOS file dialog runs
  a modal loop inside `setup()` that fires GLFW refresh callbacks, so `draw()` ran before
  `bands` existed (segfault on `bands[i]`). Fixed with a `bSetupComplete` guard at the top of
  `draw()`.
- Debugging (F5 / `lldb`) crashed on load with the Command Line Tools' LLDB (lldb-1700) on
  macOS 27; fix is updating to Command Line Tools for Xcode 27.2+ via Software Update.

The rest of this doc is the original Windows-side snapshot, kept for reference.

## What lives where (Windows)

```
of_v0.12.1_vs_64_release/          <- oF 0.12.1 VS/x64 release zip, NOT git
├── CLAUDE.md                      <- project guidance; copied into this repo as of-root-CLAUDE.md
├── addons/                        <- stock addons + 5 third-party ones (below), each its own git clone
└── apps/myApps/                   <- THIS repo: github.com/danpetry/openframeworks_apps
```

Only `apps/myApps` is version-controlled. Everything else has to be recreated.

## 1. openFrameworks itself

Download **openFrameworks 0.12.1 for macOS** (`of_v0.12.1_osx_release.zip`)
from openframeworks.cc/download. Keep the same version so the addons below
(pinned to the commits that work with 0.12.1) keep working. Install Xcode (plus
command-line tools: `xcode-select --install`).

Then clone this repo into the oF tree, replacing the stock `apps/myApps`:

```bash
cd of_v0.12.1_osx_release/apps
rm -rf myApps
git clone git@github.com:danpetry/openframeworks_apps.git myApps
cp myApps/of-root-CLAUDE.md ../CLAUDE.md   # restore root guidance (then update its Windows-specific build section)
```

## 2. Third-party addons (clone into `addons/`)

| Addon | Remote | Commit/branch used on Windows | Notes |
|---|---|---|---|
| ofxAudioFile | github.com/npisanti/ofxAudioFile | `ec426c8` | Has an `osx` section; should be fine |
| ofxFft | github.com/kylemcdonald/ofxFft | `b5208b3` | Sketches use the **KISS FFT** backend (default; FFTW libs aren't present) -> pure C, fully cross-platform |
| ofxSoundObjects | github.com/roymacdonald/ofxSoundObjects | `6f78be3` | Local changes on Windows were only to `example-ofxFft` project files - irrelevant |
| ofxFastFboReader | github.com/satoruhiga/ofxFastFboReader | `12b0069` | PBO-based readback; should work on macOS GL |
| ofxFFmpegRecorder | **github.com/danpetry/ofxFFmpegRecorder** (Dan's fork) | branch `dpetry/handle-paths-with-spaces` (`025f1d5`, pushed) | Fork fixes paths with spaces & optional output pix_fmt. Use this, not upstream |

```bash
cd of_v0.12.1_osx_release/addons
git clone https://github.com/npisanti/ofxAudioFile && git -C ofxAudioFile checkout ec426c8
git clone https://github.com/kylemcdonald/ofxFft && git -C ofxFft checkout b5208b3
git clone https://github.com/roymacdonald/ofxSoundObjects && git -C ofxSoundObjects checkout 6f78be3
git clone https://github.com/satoruhiga/ofxFastFboReader && git -C ofxFastFboReader checkout 12b0069
git clone -b dpetry/handle-paths-with-spaces git@github.com:danpetry/ofxFFmpegRecorder
```

`ofxGui` is a stock addon (ships with oF).

## 3. ffmpeg

On Windows, `ffmpeg.exe` comes from **Chocolatey** on `PATH`; sketches never
call `setFFmpegPath`, so the recorder just runs `ffmpeg`.

On Mac: `brew install ffmpeg`. Catch: an app launched from Xcode/Finder does
**not** inherit the shell's `PATH`, so `popen("ffmpeg ...")` probably won't find
`/opt/homebrew/bin/ffmpeg`. Set it explicitly in each sketch's `setup()`:

```cpp
#ifdef TARGET_OSX
    recorder.setFFmpegPath("/opt/homebrew/bin/ffmpeg");
#endif
```

(The addon also bundles `libs/ffmpeg/lib/osx/ffmpeg`, but it's an old x86_64-only
binary (would need Rosetta on Apple Silicon); prefer Homebrew's.)

## 4. Code changes the sketches need

Sketches affected: `Template_1_reactive_bounce`, `2026_07_20_grimy_electro_loop`,
`2026_07_28_soulful_glitchy_loop`, `2026_08_16_squishy_marchy` (all share the
same `setup()` boilerplate and `src/FftObject.*`).

1. **Audio API is pinned to WASAPI** (Windows-only):
   `streamSettings.setApi(ofSoundDevice::MS_WASAPI);` - this was done to stop
   RtAudio probing ASIO and waking Sonarworks. On Mac, guard it with
   `#ifdef TARGET_WIN32` (or set `ofSoundDevice::OSX_CORE`).
2. **`bin/data/lastAudioFile.txt` holds absolute Windows paths**
   (`E:\Dan music\Documents\Bounces\Live sets\For instagram\...wav`). It's in
   `bin/`, which is gitignored, so it won't come across anyway - first run will
   show the file picker and write a new one. Fine as-is.
3. **ffmpeg path** - see section 3.
4. `2025_11_26_musicclip` loads a wav by relative name via `ofToDataPath` - needs
   that file copied into its `bin/data/` (see section 6).
5. `exit()` stops/closes the stream before members get destroyed (fix for an
   access violation on shutdown). Keep it; it isn't Windows-specific.

### On `ofSoundGetSpectrum` vs the current FFT setup

Don't switch. `ofSoundGetSpectrum()` only works with `ofSoundPlayer` (the FMOD
backend). It can't see audio running through `ofSoundStream`/ofxSoundObjects, and
it gives you a fixed linear spectrum with no control over windowing or buffer
size. The current pipeline isn't Windows-specific: ofxSoundObjects on top of
RtAudio (CoreAudio on Mac), plus ofxFft with KISS. It's also had real fixes put
into it:
the mutex snapshot in `FftObject::updateBands` (fixed a data race that caused
broadband spikes), log-frequency band sampling, and per-band attack/release
smoothing. Only the `setApi` line ties it to Windows.

## 5. Project files & building

`.sln`/`.vcxproj` and `build.ps1` are Windows-only. For each sketch on Mac:

- Open **projectGenerator** (in the oF macOS zip), point it at the sketch folder
  ("Import"), and hit Update. It reads `addons.make` and generates an
  `.xcodeproj` (plus `Makefile`/`config.make` for command-line builds).
- Command line alternative: `make -j8 Release` then `make RunRelease`, or
  `xcodebuild -project <name>.xcodeproj -configuration Release`.
- Worth writing a `build.sh` equivalent of `build.ps1` once one sketch builds.
- Output lands in `bin/<name>.app`; `ofToDataPath` still resolves to `bin/data/`.
- Decide whether to commit the generated `.xcodeproj` (and add Xcode entries
  like `xcuserdata/`, `*.app`, `DerivedData/` and `.DS_Store` to `.gitignore`).
  The current `.gitignore` is the VS template.

Recommended order: get `Template_1_reactive_bounce` building and running first,
then copy the fixes into the dated sketches.

## 6. VS Code setup (how the sketches were actually built/run day-to-day)

Each sketch is opened as its **own VS Code folder** (e.g. `code apps/myApps/2026_08_16_squishy_marchy`),
not the whole myApps repo; all paths below are relative to `${workspaceFolder}` = the sketch folder.

Every sketch folder has an identical `.vscode/` with three files, copied from the
template. They're tracked in git for `2026_07_20_grimy_electro_loop`,
`2026_07_28_soulful_glitchy_loop` and `2026_08_16_squishy_marchy`, but **untracked in
`Template_1_reactive_bounce`** (commit it, since new sketches are copied from it).

### `tasks.json` (Windows)
Three shell tasks, all calling `powershell.exe -ExecutionPolicy Bypass -File ${workspaceFolder}/../build.ps1 -Project ${workspaceFolderBasename} ...`:
- **Build Debug** (default build task, Ctrl+Shift+B): `-Configuration Debug`, problemMatcher `$msCompile`
- **Build Release**: `-Configuration Release`, problemMatcher `$msCompile`
- **Clean**: `-Target clean`
- All use `presentation: { reveal: always, panel: shared, clear: true }`.

### `launch.json` (Windows)
One config, **"Build and launch (Debug)"** (F5):
`type: cppvsdbg`, `program: ${workspaceFolder}/bin/${workspaceFolderBasename}_debug.exe`,
`cwd: ${workspaceFolder}/bin`, `console: externalTerminal`, `preLaunchTask: "Build Debug"`.

### `c_cpp_properties.json` (Windows, IntelliSense only)
Config `Win32-oF-Debug-x64`: includes `src/`, all `libs/openFrameworks/*` subdirs, the
bundled `libs/*/include` dirs (glm, rtAudio, freetype, FreeImage, glew, tess2, cairo,
glfw, json, curl, pugixml, ...), and each addon's `src`/`libs` dirs (ofxAudioFile,
ofxFastFboReader, ofxFFmpegRecorder, ofxFft incl. `libs/kiss`, ofxGui, ofxSoundObjects).
Defines include `WIN32 _DEBUG POCO_STATIC GLM_FORCE_CTOR_INIT GLM_ENABLE_EXPERIMENTAL USING_OFX_SOUND_OBJECTS`;
compiler MSVC 14.44 `cl.exe`, `/std:c++latest /Zc:__cplusplus`, `windows-msvc-x64`.

### Mac equivalents to write
Keep the same task/config names and the same one-folder-per-sketch workflow:
- **tasks.json**: replace the PowerShell calls with the oF Makefile that the
  projectGenerator produces: `make -j8 Debug` / `make -j8 Release` / `make clean`
  (cwd `${workspaceFolder}`), problemMatcher `$gcc`. Or call a `../build.sh` if one gets
  written to mirror `build.ps1`.
- **launch.json**: `type: lldb` (CodeLLDB extension) or `type: cppdbg` with
  `MIMode: lldb`. Set `program: ${workspaceFolder}/bin/${workspaceFolderBasename}_debug.app/Contents/MacOS/${workspaceFolderBasename}_debug`,
  `cwd: ${workspaceFolder}/bin`, `preLaunchTask: "Build Debug"`.
- **c_cpp_properties.json**: add a `Mac` config with
  `compilerPath: /usr/bin/clang++`, `intelliSenseMode: macos-clang-arm64`,
  `cppStandard: c++17` or later, same oF/addon include paths (drop the Windows-only ones like
  `videoInput`), defines `GLM_FORCE_CTOR_INIT GLM_ENABLE_EXPERIMENTAL USING_OFX_SOUND_OBJECTS`
  plus `TARGET_OSX` if IntelliSense doesn't pick it up. Easier alternative: generate
  `compile_commands.json` (e.g. `bear -- make Debug`) and point
  `compileCommands` at it.

### Editor/user-level settings (not project-specific, recreate or use Settings Sync)
- Extensions: `ms-vscode.cpptools`, `ms-vscode.cpptools-extension-pack`,
  `ms-vscode.cpptools-themes`, `ms-vscode.cmake-tools`, `ms-vscode.cpp-devtools`,
  `anthropic.claude-code`. On Mac also add `vadimcn.vscode-lldb` (CodeLLDB).
- User `settings.json`: `editor.fontSize: 12`, `editor.fontFamily: "Nerd"` (a Nerd Font;
  install it on the Mac), `workbench.colorTheme: "Monokai"`,
  `claudeCode.preferredLocation: "panel"`.

## 7. Files NOT in git that need copying by hand

`bin/` is gitignored, so audio assets and recordings don't travel with the repo:

| Path | Size | What |
|---|---|---|
| `2025_11_26_musicclip/bin/data/Master rec 0010 [2025-11-26 083611] - Excerpt for IG.wav` | ~180MB folder | required by that sketch |
| `2025_10_17_musicclip/wav/` (untracked) + `2025_10_17_musicclip/2025_10_17_of-and-modular.mp4` | ~105MB | source audio / exported clip |
| `*/bin/data/*.mp4` | various | previous exports - copy only if wanted |
| `E:\Dan music\Documents\Bounces\Live sets\For instagram\*.wav` | - | source wavs the newer sketches pick via file dialog |

Untracked repo clutter to clean up rather than carry over (per CLAUDE.md):
`2025_11_26_musicclip/cpp src/` (stray copy of `ofApp.cpp`),
`mySketch/` (scratch). (Don't delete `Template_1_reactive_bounce/.vscode/`. It's
untracked but it's the source of the VS Code config, see section 6.)

## 8. Claude Code context

- Root `CLAUDE.md` -> restored from `of-root-CLAUDE.md` (step 1). Update its
  "Build & run" section for Mac once that works.
- Claude memory directory on Windows was empty - nothing to move.
