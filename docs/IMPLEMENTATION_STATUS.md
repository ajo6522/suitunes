# V1 feature status — working prototype 0.2.0

References are to the supplied 27-page `Suitunes-V1.0.0-dev-spec.pdf`. “Implemented” means prototype code and the stated checks exist; musical/host acceptance remains separate.

| Feature | Pages | Implementation and remaining acceptance |
|---|---|---|
| Auto Tune monophonic correction | 1–3, 7–11 | Implemented; sine/harmonic tests. Real-vocal corpus/listening still open. |
| Key / major / minor / chromatic / custom | 2–4 | Implemented, 12 note-class buttons; empty mask leaves pitch unchanged. |
| Central Speed/Strength rotary controls | 3–4 | Implemented; scalable 2:1 blue dashboard. |
| Pitch/target display | 3–4 | Actual F0, estimated target, confidence and audio history. Target is not independently measured output F0. |
| Formant preserve/adjust | 3, 9–10 | Spectral/offline implementation. Disabled in low-delay engines; low-delay preservation remains unimplemented. Vocal ±3-semitone quality gate unverified. |
| Vibrato preserve/reduce/enhance | 3, 10–11 | Prototype residual preservation/reduction and enhancement (maximum ±20 cents at selected Rate). Listening open. Detailed offline envelope editor disabled. |
| Drift correction | 3, 10–11 | Prototype slow/fast centre decomposition; listening open. |
| Confidence / hysteresis / voicing | 8–11 | YIN confidence, amplitude gate, boundary dead band and unvoiced relaxation. Production octave continuity/breathy corpus acceptance open. |
| MIDI target / held notes | 4, 12 | Event offsets segment processing; last-held note across channels; Note-On/Off/All Notes Off; no note relaxes to unity. Processor pitch test passes; FL routing unverified. |
| Fine Tune WAV analysis | 4–5, 11 | Background import, source, F0 and segmentation; mono/stereo 8–192 kHz, 512 MiB decoded-source cap. Segmentation needs real vocals. |
| Note regions / trace / waveform | 4–5 | Procedural blue editor; cached drawing; no piano keyboard. |
| Selected-region Auto Fine Tune | 5–6, 19 | Implemented; selection-boundary splits. Test proves samples outside selection are bit-exact. |
| Select All | 5, 19 | Implemented; initial import selects full file. |
| Pointer-centred horizontal zoom | 5, 19 | Implemented; bounded to file. Native/trackpad usability still needs testing. |
| Pitch drag / split / undo / redo | 5–6, 19 | Implemented, original-based render. Exact undo/redo model tests pass. Double-click/Split divides a region. |
| Before/After | 5–6, 19 | Original/Corrected preview through common delay path. Host/device audition unverified. |
| WAV full/selected export | 3, 13 | Source rate/channels; 24-bit PCM/32-bit float; sample count/outside-edit preservation tested; no normalization. |
| MIDI export | 3, 12–13 | Type 0, 960 PPQ, explicit 120 BPM; edited/source pitch, times, RMS-derived velocity, overlap trimming. Round-trip tests; FL piano-roll comparison open. |
| MIDI import | 3, 12 | Type 0/1, first nonempty note track; separate tempo/note-track test passes. Multi-track picker not implemented (first-track rule shown). |
| Custom presets only | 3, 6 | Save/load/recent-file dropdown; no factory preset distribution. |
| Portable STS | 6–7, 14, 19 | UTF-8 JSON, `SUITUNES_STS`, `SuitunesPreset`, formatVersion 1. Unknown optional fields ignored; validation/size cap/errors. Legacy XML migration retained. |
| Preset persistence | 19 | Disk/settings/host round trips tested. Recent file list is host metadata; arbitrary `.sts` can be loaded. |
| Separate host/editor state | 7, 14 | Host parameters + edit metadata + external source reference. STS excludes source/edits and preserves imported session. Keep source WAV; full FL reopen acceptance open. |
| Replaceable DSP | 7, 14–15 | Separate components plus original baseline for experiments; concrete headers rather than exact proposed directory layout. |
| Real-time safety | 15–16 | Cached atomics, fixed slices/preallocation, no callback file/state parsing. Steady-state C++ `new` guards pass all engines and processor. Not every native allocator/host function is instrumented. |
| Parameter / key / MIDI smoothing | 16, 18 | Ratio/mix/formant/trajectory smoothing and dead band; audible-click listening gate still open. |
| Variable blocks | 16 | 0/1/16/32/64/127/128/256/512/1024/4096 and changing sequences tested. |
| True latency / PDC | 2, 16–17 | Live 10 ms (110+ Hz), Full voice ~16 ms (65+ Hz), Spectral 120 ms; unity impulse/dry alignment tested. Full-range 15 ms ceiling and FL PDC/loopback acceptance unmet/unverified. |
| 30-minute soak / annotated vocals | 18–19 | NOT completed. Synthetic checks are not substitutes. |
| Windows VST3 | 11, 20 | MSVC x64 build + pluginval level 10; FL manual acceptance not recorded. |
| macOS VST3/AU | 11–12, 21 | CMake targets enabled; no native compile/AU validation/signing/host checks here. |
| Comparative experiments | 20–22 | Signalsmith window/hop and before/after CPU logged. aubio reference corpus, Rubber Band/SoundTouch, blind shifter comparison and Pitcher A/B still open. No GPL alternative embedded. |
| Logo / icon assets | 22–23 | Procedural scalable wordmark/controls/plots; unchanged reference PNGs. Approved master SVG/icon export package still open. |

## Provisional choices

Smaller Speed milliseconds is faster; Strength is continuous percent. Vibrato/drift laws are experimental. Change engine while stopped. Low-delay formants are unavailable. Live declines input below 110 Hz. First MIDI note track is used; no picker. MIDI export explicitly uses 120 BPM, not inferred song tempo. Source WAVs remain external and must be kept available.

This is not complete V1 acceptance. Disabled controls are explicitly identified rather than presented as working DSP.
