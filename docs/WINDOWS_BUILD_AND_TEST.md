# Windows x64 build and test - Pitch Pop 0.3

1. Install Visual Studio 2022 or newer with Desktop development with C++, an x64 MSVC toolset, Windows SDK and C++ CMake tools. Use Developer PowerShell. Git and CMake 3.24+ must be on PATH.
2. Extract the source to `C:\Dev\PitchPop`. First configure needs internet for pinned JUCE 8.0.8 and Signalsmith Linear 0.6.6; Signalsmith Stretch is vendored. Existing JUCE can be passed using `-DSUITUNES_JUCE_DIR=C:/Dev/JUCE`.
3. Build in a fresh folder; do not reuse a folder with a different generator:

```powershell
cd C:\Dev\PitchPop
cmake -S . -B build-vs2022 -G "Visual Studio 17 2022" -A x64
cmake --build build-vs2022 --config Release --target Suitunes_VST3 Suitunes_Standalone SuitunesTests PitchPopRegression PitchPopDiagnostics SuitunesBench --parallel 4
ctest --test-dir build-vs2022 -C Release --output-on-failure
```

For Visual Studio 2026 use `-G "Visual Studio 18 2026"` and a separate `build-vs2026` directory. Internal target names retain Suitunes for compatibility. Outputs are `build-vs2022\Suitunes_artefacts\Release\VST3\Pitch Pop.vst3` (the entire directory) and `...\Standalone\Pitch Pop.exe`. The tested toolchain here was MSVC 19.51 / VS Build Tools 2026 / SDK 10.0.28000.0.

4. Preserve the current source commit and installed Suitunes bundle. Close FL and the standalone. Pitch Pop preserves the old VST3 identity: do not leave both old Suitunes and new Pitch Pop copies in scanned folders. Archive the old bundle outside all VST search paths, keeping it available for rollback. In administrator PowerShell run the supplied `Install-Windows.ps1` against the whole new bundle. The script refuses an old-name duplicate rather than silently replacing it.
5. In FL use Options > Manage plugins, Verify plugins, Rescan previously verified plugins, then Find installed plugins. Alternatively add the development VST3 output folder as a search path for testing, after temporarily excluding the old copy. Confirm the new green Pitch Pop UI and Quality mode. Test in a new project before reopening saved work. Build outputs are not automatically installed. A machine without development tools may need the Microsoft Visual C++ x64 runtime.
6. Put Pitch Pop first on a clean monophonic vocal Mixer insert. Set G / Minor, verify G A Bb C D Eb F enabled, Strength 100%, Speed 40 ms, wrapper mix 100%. Correction is already enabled. Quality uses formant compensation and 150 ms audio delay; Live is experimental 10 ms with input >=110 Hz, Full voice experimental 16 ms >=65 Hz. Change engines while stopped.

## Required FL keyboard and regression check

Record FL version, driver, sample rate, block size, detached/bridged/Patcher status and wrapper keyboard-priority settings. Test direct 64-bit VST3 first. Auto Tune value displays are read-only: use knob dragging to adjust them, avoiding dynamically-created text editors that can capture transport. Fine Tune retains numeric editing. Image-Line documents host/plugin key priority and special bridging/Patcher focus behavior in its [wrapper manual](https://www.image-line.com/fl-studio-learning/fl-studio-online-manual/html/plugins/wrapper.htm).

- Click Auto Tune background, Speed, Strength, a note button, Key/Scale and Engine controls in turn (close any popup). Press space twice after each: FL transport must start and stop, and the last control must not be activated by space. Test after switching back from Fine Tune and after loading an old host project/preset with enabled=0. Auto Tune numeric text entry and open popup menus need explicit checks too.
- Import WAV in Fine Tune; select a range and Auto Fine Tune it. Verify outside-range audio unchanged, Select All, pointer-centred wheel zoom, pitch drag, split, undo/redo, Original/Corrected preview and WAV/MIDI exports. Press space to preview; switch back to Auto Tune and verify FL owns space again.
- Save/reopen a test FL project with edits and the original source WAV still present. Load both legacy and current `.sts`; presets must not replace the imported source/edits. Host bypass must remain functional; the obsolete Correction flag must have no effect.
- Test 44.1/48/88.2/96 kHz and 32/64/128/256/512/1024 buffers as supported, changing buffers, automation, MIDI at nonzero sample offsets, stop/start/seek/loop, offline export, tails and 30-minute soak. Record dropouts and FL CPU separately from the offline benchmark.
- Compare active unity impulse and bypass renders, measured offset and reported plugin latency. Measure microphone loopback separately; do not call the plugin's 10 ms setting total monitoring latency.

**Status:** component focus test passes. New-build FL keyboard/PDC/listening test is still pending; see audit for the failed folder-picker automation attempt.

## Objective diagnostics and benchmark

`PitchPopDiagnostics` is an offline console tool: input WAV, output directory, mode (0 Full voice / 1 Live / 2 Quality), Speed ms, Strength 0..1, preserve-formants 0/1. It writes latency-aligned PCM24 `corrected.wav` and `trace.csv`. It uses G natural minor/A4=440. All disk I/O is outside the plugin callback. Use separate output directories per run.

```powershell
build-vs2022\PitchPopDiagnostics_artefacts\Release\PitchPopDiagnostics.exe C:\Audio\dry.wav C:\Audio\quality 2 40 1 1
build-vs2022\PitchPopDiagnostics_artefacts\Release\PitchPopDiagnostics.exe C:\Audio\dry.wav C:\Audio\no-formants 2 40 1 0
build-vs2022\Release\SuitunesBench.exe
```

Trace rows have detector sample position, RMS, confidence, onset, candidate Hz/confidences, chosen F0/MIDI, octave rejection, G/minor mask, actual target MIDI, requested/applied cents, ratio, formant mode and latency. Controller values are sampled at the end of a <=64-sample offline slice; timestamps identify the last detector hop, so applied-trajectory timestamps have up to 63 samples of observation skew. No live CSV logging is enabled.

For reproducible known-F0 audio install Python 3.10+ and NumPy (`python -m pip install numpy`), then:

```powershell
python tools\Measure-Pitch.py generate build-vs2022\corpus
build-vs2022\PitchPopDiagnostics_artefacts\Release\PitchPopDiagnostics.exe C:\Dev\PitchPop\build-vs2022\corpus\chromatic-g-minor.wav C:\Dev\PitchPop\build-vs2022\quality-check 2 40 1 1
python tools\Measure-Pitch.py measure build-vs2022\quality-check\corrected.wav build-vs2022\corpus\reference.json build-vs2022\quality-check
```

Use absolute paths for the C++ diagnostic tool. `metrics.json` and `output-f0.csv` use independent normalized autocorrelation rather than the plugin's detector. Held-note evaluation excludes transitions explicitly. Compare the whole audio including attacks by listening; synthetic stable-frame results do not cover vocal quality. Optional `pluginval --strictness-level 10 --validate "...\Pitch Pop.vst3"` tests compatibility, not musical acceptance.
