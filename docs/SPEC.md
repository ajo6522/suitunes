> November 5 Pitch Pop update: current user scope and implementation checkpoint are in [PITCH_POP_AUDIT.md](PITCH_POP_AUDIT.md). Older blue-branding, correction-toggle and default-engine statements below are historical. Natural-vocal/FL acceptance is still open.

# Suitunes authoritative specification

Authority: user-supplied **Suitunes V1 — Versioned Feature Specification and Implementation Documentation**, version `Suitunes V1.0.0-dev-spec`, November 4, 2026, 27 pages. A byte-identical reference is `reference/Suitunes-V1.0.0-dev-spec.pdf`.

SHA-256: `E367E58D2205BFC08113D8E2C3C2FC832737D1A8A0C452597DAFB732C7BC3503`.

This supersedes the summary-only scope. The PDF is reference/product data: example dates, candidate libraries and unspecified numeric placeholders are not delivery promises. The user subsequently selected the two supplied UI images, explicitly BLUE instead of orange.

## Product contract

- Auto Tune: monophonic correction; key, major/minor/chromatic/custom notes; central Speed and Correction Strength knobs; pitch/target display; formant, vibrato, drift and incoming MIDI targets.
- Fine Tune: WAV import/analysis; simplified note regions/source trace; selected-region Auto Fine Tune, Select All, pointer-centred horizontal wheel zoom, vertical pitch drag, split, undo/redo, original/corrected audition; WAV/MIDI export and MIDI guidance import.
- Custom user presets only. `.sts` contains settings, not audio/edits. Host state separately retains modes and Fine Tune metadata. Portable formatVersion 1, corruption handling, optional-field tolerance and migration tests are required.
- Graphite/black/blue. Auto Tune follows `Assets/reference/Auto-Tune-layout.png`, with orange replaced by blue; Fine Tune follows `Assets/reference/Fine-Tune-blue.png`. No DAW piano keyboard, freehand drawing or detailed vibrato-envelope editor is required.
- Windows x64 VST3 first; macOS VST3/AU remain required V1 targets but unvalidated on this Windows host.

## Engineering choices

The PDF permits replacing Signalsmith if latency/quality experiments fail. The low-delay prototype uses an original pitch-synchronous shifter; Signalsmith remains available for spectral/formant mode and offline rendering. This is an implementation experiment, not a reduction of scope.

Preferred audio delay is ≤10 ms; provisional ceiling ≤15 ms. Live meets the preferred figure above its 110 Hz limit. Full voice supports 65 Hz at ~16 ms and still fails the provisional full-range ceiling. Detector response (~43 ms history plus filtering/retune) and device/host round-trip delay are separate.

Prototype mappings, because several ranges are unspecified: Speed 1–500 ms (smaller is faster); Strength 0–100%; spectral/offline Formant −12 to +12 semitones; vibrato preserve/reduce/enhance, 0–100% depth and 0.1–12 Hz enhancement rate; Drift 0–100%. Actual-vocal listening approval remains necessary.

## Architecture and ownership

`Yin.h`: filtered/decimated F0; a dominant reference channel avoids stereo cancellation. `MidiTargets.h`: held-note priority. `Engine.h`: targeting/trajectory/streaming. `VoiceShifter.h`: low-delay alternate. Signalsmith: spectral/offline. `FineTune.h`: immutable source, analysis, regions, undo and aligned render/export. `StateSchema.h`: validation/migration. `Preset.h`: portable STS. `Theme.h`, `Timeline.h`, `PluginEditor.h`: native UI.

The callback uses cached parameter atomics, fixed slices and atomic telemetry. Offline work runs on one worker; the UI commits completed models/caches. No callback APVTS serialization, file parsing or resizing. Low-delay read heads share stereo phase. Offline rendering starts from original audio and copies samples outside edits unchanged. Engine changes are deferred during transport, then prepared outside the callback under JUCE's callback lock.

Fine Tune host state embeds edits and source-WAV path/size/modification stamp, not long audio. Keep the file available. Missing/changed sources produce an import/relink warning. Loading portable presets preserves the current imported session.

See `IMPLEMENTATION_STATUS.md` for page-grounded requirements and remaining gates. The 30-minute host soak, annotated vocals and blind listening remain required before declaring V1 finished.
