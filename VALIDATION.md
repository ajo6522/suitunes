# Validation — prototype 0.2.0, November 4, 2026

## User listening result — November 5, 2026

The user tested the prototype and reported **warbling and wrong notes**. This is a failed vocal-quality result. The recording, engine mode and settings have not yet been supplied, so no cause is confirmed. Earlier automated results below remain numerical/compatibility evidence only. See `docs/NEXT_SESSION.md` for the next investigation.

Windows x64, MSVC 19.51 / Visual Studio Build Tools 2026, Windows SDK 10.0.28000.0, JUCE 8.0.8, Release. Test machine reports Intel Core i7-14700F, 20 cores / 28 logical processors. No minimum supported processor has been established.

## Efficiency measurements

`SuitunesBench` renders three seconds of generated stereo harmonic audio per case and times only engine processing. Percent is wall time divided by the audio duration for **one core**; it is not total computer or FL CPU. Source generation is outside timing. OS scheduling can affect maxima. The original source is preserved in `Tests/BaselineEngine.h` for repeatability.

| 48 kHz / 128 samples | Original 0.1 engine | Current Full voice | Current Live | Spectral/formants |
|---|---:|---:|---:|---:|
| Audio delay | 120 ms | 16 ms | 10 ms | 120 ms |
| One-core processing budget | 33.142% | 0.672% | 0.684% | 1.662% |
| 99th-percentile block time | 2066.2 us | 33.6 us | 33.4 us | 363.3 us |

On this test, Full voice uses about **98% less processing time** (~49× faster) and has **87% less audio delay** than the original build. Live has **92% less delay** over its supported input range. These are local synthetic benchmark results, not a claim of maximum possible efficiency or proven vocal quality.

Full sample-rate/buffer distributions are `evidence/benchmark-final.log` in the source ZIP. The experimental Signalsmith window/hop sweep is `evidence/benchmark-sweep.log`; smaller spectral windows left substantial small-shift pitch error and were rejected as the only live engine.

## Automated checks

CTest covers:

- YIN sine accuracy at 82.41/220/440/880 Hz and 44.1/48/88.2/96 kHz; silence rejection.
- Harmonic source correction at ±30 cents across supported ranges and rates; worst measured low-delay error **4.55 cents**. Separate A4 +30-cent test produces **439.988 Hz** against 440 Hz.
- Mono/stereo dry timing; block sizes 0/1/16/32/64/127/128/256/512/1024/4096; changing block sequences; wet-unity impulse delay; finite outputs and coherent stereo for the low-delay shifter. Right-only and opposite-polarity stereo detection/correction are also tested.
- Live rejects correction below 110 Hz. Custom-note targeting and empty-mask behavior are checked.
- Held MIDI note priority/all-notes-off, plus processor-level generated MIDI causing a known target pitch.
- Instrumented ordinary/aligned C++ `new` calls: zero in warmed steady-state processing for all three engines during parameter changes and in the processor callback. Native malloc, external host code and first-use behavior are not exhaustively instrumented.
- JSON STS formatVersion 1, unknown optional metadata, invalid/future formats, legacy XML migration, disk load/save, malformed-file refusal and host parameter restoration.
- Fine Tune WAV import/segmentation, selected correction, exact Undo/Redo metadata, original-based rendering, unchanged samples outside selection, unchanged full duration, 24-bit PCM/32-bit float, selected duration, MIDI export/reimport Type 0 and Type 1 guidance, and edit-metadata round trip.

The low-delay shifter uses fixed centre latency with pitch-synchronous read excursions around that centre. Unity/no-shift impulse alignment verifies the reported delay. Pitch-shifted transients can move/smear within grains; a single constant group-delay claim does not describe every transformed transient.

## Interface and host validation

Both editor views are rendered from the compiled JUCE components and visually checked. Preview audio is a generated test fixture, not a real-vocal quality demonstration. Actual knobs, selection and editor components are used; PNG references are not a fake interactive background.

pluginval strictness level 10, GUI checks enabled, 60-second timeout: **SUCCESS** against the Windows VST3. Its separate Steinberg-validator step is skipped because no validator executable was configured. Final output is `evidence/pluginval-10.log`; the earlier level-5 log is retained too.

**FL Studio manual testing is not completed here.** The user's earlier opened build was 0.1; do not count that as validation of the new UI/DSP. Follow README for update/rescan, MIDI ports, source restoration, PDC and actual-vocal listening. No 30-minute real-time soak, annotated vocal accuracy/95% note-identity result, blind Pitcher comparison or native macOS/AU validation is claimed.

## Remaining product gates

Full voice remains above the PDF's provisional 15 ms ceiling. Live meets 10 ms only for input ≥110 Hz; detector history (~43 ms), correction trajectory and device buffers add response/monitoring delay. Low-delay formant preservation is unimplemented. The spectral/offline formant and vibrato/drift mappings are experimental and require vocal listening. Production octave continuity, advanced Fine Tune timing/envelope editing, MIDI track picker, dedicated logo master assets and signed installers are not complete. See the full feature matrix for exact status.
