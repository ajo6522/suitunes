# Pitch Pop 0.3 - pitch-correction repair checkpoint

Pitch Pop (formerly Suitunes / Sweet Tunes) is a JUCE/C++ monophonic vocal pitch corrector with Auto Tune and an original-based Fine Tune editor. Green soda branding, native knobs and both functional views are preserved. This is a testable engineering checkpoint; natural-vocal quality and Pitcher parity are not yet accepted.

Read [the audit](docs/PITCH_POP_AUDIT.md), [Windows build/test guide](docs/WINDOWS_BUILD_AND_TEST.md), [Pitcher A/B procedure](docs/PITCHER_AB_PROCEDURE.md) and [measured validation](VALIDATION.md).

- Auto Tune correction is always enabled. Set Key/Scale, Speed and Strength. The removed Correction toggle's legacy parameter is retained but ignored for saved-project compatibility. Host bypass still works.
- New instances default to Quality: Signalsmith spectral resynthesis with formant compensation, **150 ms plugin audio delay at 48 kHz**. This is a quality evaluation mode with substantial monitoring delay.
- Experimental Full voice: 16 ms / 65+ Hz. Experimental Live: 10 ms / 110+ Hz. These custom low-delay shifters have **no formant preservation** and need real-vocal listening. Existing saved engine choices are retained. Change mode while stopped.
- Detector candidates, bounded octave continuity, onset gating and persistent target hysteresis/dwell replace first-candidate-only decisions. Retune timing uses detector hops and sample-time smoothing.
- Fine Tune keeps WAV import/background analysis, selected-region Auto Fine Tune, Select All, pointer-centred wheel zoom, note drag/split, exact undo/redo, Original/Corrected preview, WAV/MIDI exports and MIDI guidance. Keep the original WAV available for saved-project restoration.
- Portable `.sts` stays formatVersion 1, using the legacy wire signature for compatibility. New metadata identifies Pitch Pop; old JSON/XML fixtures remain readable. Presets do not embed/replace source audio or edits.
- Auto Tune declines keyboard focus and does not handle space. Fine Tune may use space for preview. Component tests pass; **actual FL transport forwarding remains unverified** because the host folder-picker automation failed before loading the new build.

## Build on Windows

Use Developer PowerShell with Visual Studio 2022 Desktop development with C++, Windows SDK, Git and CMake 3.24+:

```powershell
cmake -S . -B build-vs2022 -G "Visual Studio 17 2022" -A x64
cmake --build build-vs2022 --config Release --target Suitunes_VST3 Suitunes_Standalone SuitunesTests PitchPopRegression PitchPopDiagnostics SuitunesBench --parallel 4
ctest --test-dir build-vs2022 -C Release --output-on-failure
```

For VS 2026 use `Visual Studio 18 2026` and a separate build directory. First configure downloads pinned JUCE 8.0.8 and Signalsmith Linear 0.6.6; Stretch is vendored. Internal target IDs keep Suitunes for compatibility; binaries display Pitch Pop.

Whole VST3 bundle: `build-vs2022/Suitunes_artefacts/Release/VST3/Pitch Pop.vst3`. Standalone: `.../Standalone/Pitch Pop.exe`. Do not copy just the binary inside the VST3 directory. Close hosts, preserve/disable the old scanned Suitunes bundle, then install/rescan according to the detailed guide. The VST3 identity is preserved, so avoid duplicate old/new copies in search paths.

## Verified versus pending

The automated suites cover deterministic G-minor mapping, independent fixed-shift direction/output pitch, multiple rates and changing buffers, controller trajectory, noise rejection, C++ allocation guards including resets, legacy presets, Fine Tune selected-sample preservation/export and component focus behavior. Strictness-10 pluginval passes. The independent output checker reports results on generated steady harmonic notes, not natural-vocal acceptance.

Still required: the user's dry failure excerpt and original settings; annotated vocal accuracy/voicing/formant tests; blind matched-input Pitcher listening; actual FL space/PDC/MIDI/export and 30-minute soak; macOS build/AU validation. No proprietary algorithm duplication is promised. See the audit for exact limitations.
