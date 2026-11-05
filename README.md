# Suitunes 0.2 — blue UI and efficient DSP prototype

**Listening checkpoint, November 5, 2026:** the user reports warbling and wrong-note correction. The current engine is not accepted for vocal use. Synthetic tests and pluginval success did not establish musical quality. Start the next development session with `docs/NEXT_SESSION.md`.

Built from the user's **Suitunes V1.0.0-dev-spec** PDF and the two supplied interface references. Both screens use graphite/black and BLUE/CYAN. Speed and Correction Strength are the main rotary controls. The source ZIP contains the specification, unchanged images and feature matrix in `docs/` and `Assets/reference/`. The binary ZIP also includes `docs/` and compiled-UI previews.

**This is a working prototype, not completed V1 acceptance.** Actual-vocal quality, the full-range latency ceiling, low-delay formant preservation, a 30-minute host soak and macOS validation remain open. See `docs/IMPLEMENTATION_STATUS.md` and `VALIDATION.md`.

## What works

- Auto Tune: mono/stereo pass-through, original YIN detection, key/scale/custom notes, Speed and Strength, vibrato/drift prototype controls, MIDI last-held-note targeting and pitch/confidence display.
- Blue native UI: working rotary knobs, note buttons, preset dropdown/save/load, Auto Tune/Fine Tune views, actual signal history and level meters; resizes from 960×480 to 1920×960.
- Fine Tune: background WAV analysis, note regions/source trace/waveform, selection, pointer-centred wheel zoom, vertical note drag, Split/double-click, exact-model Undo/Redo, Original/Corrected preview, aligned WAV export and MIDI import/export.
- Portable custom `.sts`: UTF-8 JSON, signature and formatVersion 1, validated before load, optional-field tolerance. Legacy prototype XML remains readable. No factory presets.
- Host state includes parameters, view, edits and source-WAV reference. Keep the original source WAV available. Moving/changing it requires re-import/relink; the audio is not embedded in a preset/project blob.

## Choose an engine

| Engine | Reported delay at 48 kHz | Input range / tradeoff |
|---|---:|---|
| Full voice (default) | 768 samples / 16 ms | 65–1000 Hz; pitch-synchronous prototype; low-delay formant processing unavailable. |
| Live | 480 samples / 10 ms | 110–1000 Hz; lower pitches relax toward unity instead of being falsely corrected. |
| Spectral | 5760 samples / 120 ms | Signalsmith quality preset with formant preservation/adjustment; useful for recorded-vocal comparison. |

Change engine **while playback is stopped**. Changes requested during transport are deferred. Formant is disabled in low-delay Auto Tune engines and enabled in Spectral and offline Fine Tune. These are actual limitations, not fake controls.

Legacy 0.1 projects/XML presets migrate to Spectral, the original engine. Select Full voice or Live while stopped to use the optimized low-delay engine.

The detector observes about 43 ms of history and updates near 5 ms intervals after filtering. Its response and Speed smoothing are additional correction-response time, not extra audio delay lines. Audio-interface buffers and FL routing add monitored round-trip delay. Live's 10 ms figure is not a claim of 10 ms total microphone latency. Full voice still exceeds the PDF's provisional 15 ms ceiling.

## Compile on Windows, step by step

1. Install Visual Studio **2022 or newer**, choosing **Desktop development with C++**, an x64 MSVC toolset, a Windows 10/11 SDK and **C++ CMake tools for Windows**. Install Git for Windows. CMake 3.24+ is required.
2. Extract the source ZIP to `C:\Dev\Suitunes`. Open **Developer PowerShell for Visual Studio**. Check `cmake --version` and `git --version`.
3. For Visual Studio 2022:

   ```powershell
   cd C:\Dev\Suitunes
   cmake -S . -B build-vs2022 -G "Visual Studio 17 2022" -A x64
   cmake --build build-vs2022 --config Release --target Suitunes_VST3 Suitunes_Standalone SuitunesTests SuitunesBench --parallel 4
   ctest --test-dir build-vs2022 -C Release --output-on-failure
   ```

   For Visual Studio 2026 use a different build folder, e.g. `build-vs2026`, and `-G "Visual Studio 18 2026"`. Never change generator in the same build folder. This delivery was compiled with MSVC 19.51 / Visual Studio Build Tools 2026.
4. First configure downloads pinned JUCE 8.0.8 and Signalsmith Linear 0.6.6, requiring internet. Stretch is included in `vendor/`. Optional existing-JUCE override: `-DSUITUNES_JUCE_DIR=C:/Dev/JUCE` (JUCE 8.0.8).
5. Find the **whole bundle** at `build-vs2022\Suitunes_artefacts\Release\VST3\Suitunes.vst3`. Do not copy only the executable inside it.
6. For the Visual Studio editor, open the generated solution, select **Release / x64** and build **Suitunes_VST3**. After editing code, repeat the build command. Close hosts before replacing an installed binary.
7. Run `build-vs2022\Suitunes_artefacts\Release\Standalone\Suitunes.exe` for development; select audio devices. Use headphones for microphone monitoring.
8. Optional CPU benchmark: `build-vs2022\Release\SuitunesBench.exe`. It compares the preserved 0.1 engine to the current engine and prints sample-rate/buffer timing. These are percentages of one core's audio deadline, not FL's total CPU meter.

On macOS, CMake enables VST3, AU and standalone targets. Configure with an appropriate local generator/toolchain. No macOS build or AU acceptance is claimed here.

## Install/update and rescan in FL Studio

1. **Save your project and close FL Studio** and Suitunes standalone. Extract the Windows binary ZIP. Copy its entire `Suitunes.vst3` directory into `C:\Program Files\Common Files\VST3\2of3`, replacing the old bundle. Windows may request administrator rights. The included `Install-Windows.ps1` performs this copy when run as administrator. Do not install a second old copy elsewhere.
2. For your own build, administrator PowerShell from the project folder:

   ```powershell
   New-Item -ItemType Directory -Force 'C:\Program Files\Common Files\VST3\2of3' | Out-Null
   Copy-Item -LiteralPath '.\build-vs2022\Suitunes_artefacts\Release\VST3\Suitunes.vst3' -Destination 'C:\Program Files\Common Files\VST3\2of3' -Recurse -Force
   ```

   Build outputs are not installed automatically. A computer without development tools may need the Microsoft Visual C++ x64 redistributable.
3. In 64-bit FL Studio, open **Options → Manage plugins**. Enable **Verify plugins**, **Rescan previously verified plugins**, and, if needed, **Rescan plugins with errors**, then **Find installed plugins**. Confirm Suitunes is a VST3 effect. [Image-Line installation guide](https://www.image-line.com/fl-studio-learning/fl-studio-online-manual/html/basics_externalplugins.htm).
4. Add it to a **Mixer effect slot** on a clean vocal channel. The new build has blue rotary knobs and Auto Tune/Fine Tune tabs; if you still see generic sliders, FL loaded the old copy. Keep the FL wrapper mix at 100% and use the menu/controls inside Suitunes.
5. Correction starts OFF for pass-through verification. Play audio and check Hz/confidence. Turn Correction ON, set song Key/Scale, try Speed 40–100 ms and Strength 100%. Smaller Speed values produce harder tuning. Choose Live only if the vocal stays above 110 Hz.
6. Test 44.1/48/88.2/96 kHz at 32/64/128/256/512/1024 buffers where supported; automate Speed/Strength, stop/start/seek, render and inspect alignment. Verify host-reported latency/PDC separately from microphone loopback delay.

## MIDI target control in FL

1. Open Suitunes' wrapper settings and assign a MIDI **Input port**, e.g. 10.
2. Add FL's **MIDI Out** channel and set its port to the same number.
3. Enable **MIDI Control** inside Suitunes. Play MIDI notes/piano-roll melody while a monophonic vocal is routed through the Mixer insert.
4. The most recently pressed held note wins; releasing it returns to the earlier held note. With no held note, correction relaxes toward unity. Velocity currently does not change correction strength. Pitch bend/MPE/sustain-pedal behavior is not implemented.
5. Actual FL MIDI-routing/timing acceptance still needs to be recorded; processor-level generated-MIDI tests pass.

## Fine Tune workflow

1. Choose **Fine Tune**, then **Import WAV**, or drag a WAV onto the plug-in. Mono/stereo, 8–192 kHz, up to 512 MiB decoded audio. Analysis runs in the background.
2. Drag empty timeline space (or Shift-drag) to select a time range. **Select All** selects the full source. **Auto Fine Tune** corrects only intersecting material, splitting notes at selection boundaries. Unselected samples stay unchanged.
3. Drag a note vertically to change target pitch. Use **Split** or double-click to divide a region. Undo/Redo restores the model and rerenders from the original; it does not repeatedly process the previous result.
4. Wheel zoom is horizontal and centred around the pointer. **Snap to Scale** snaps manual movement to an allowed note; turn it off for continuous pitch movement.
5. **Play/Stop** auditions through the plug-in output; choose **Original** or **Corrected** for A/B. Stop before changing engine. This is manual preview, not host-synchronised timeline playback. In Fine Tune, incoming live audio passes dry; imported edits are heard by preview or exported WAV, not automatically inserted into the host arrangement.
6. Export WAV defaults to 24-bit PCM and the full file. The top-right Menu offers 32-bit float and selected-range export. Source rate/channels and time length are preserved; no normalization is added.
7. Export MIDI writes edited/source notes, times and RMS-derived velocities in a Type 0 file, 960 PPQ, with an explicit **120 BPM** map. It does not infer song tempo or export continuous vibrato/pitch-bend curves.
8. Menu → **Import MIDI melody** accepts Type 0/1 and uses the first nonempty note track as guidance. A track picker is not yet implemented.
9. Keep the original WAV with your project. Host state stores its path/size/modification stamp and edit metadata. Missing/changed sources are reported rather than silently replaced. Timing edits and detailed vibrato-envelope editing remain disabled.

Prototype shift limits: Auto Tune ±12 semitones; Fine Tune ±24 semitones. MIDI guidance beyond these limits is clipped to the render engine's supported shift; it is not a promise of arbitrary transposition.

## Source map, evidence and licensing

See `docs/SPEC.md`, `docs/IMPLEMENTATION_STATUS.md`, `VALIDATION.md`, `Source/`, `Tests/` and `preview/`. The source ZIP includes build/test/pluginval/benchmark evidence. `tools/Package.ps1 -BuildDir build-vs2022` packages a locally validated build; copy evidence logs to the documented project paths first if using a different build folder.

JUCE is AGPLv3/commercial: obtain the appropriate license before proprietary distribution. Signalsmith Stretch and Linear are MIT; preserve notices. No aubio, Rubber Band or SoundTouch code is embedded. This is an unsigned development build, with no commercial-release claim. Synced files under the enclosing project's `sources/` were not modified.
