# Pitch Pop 0.3 validation - November 5, 2026

This repair checkpoint is not natural-vocal acceptance. Historical 0.2 results follow below and do not describe the current default engine.

## Current build and objective results

Windows x64 Release, JUCE 8.0.8, MSVC 19.51 / VS Build Tools 2026, Windows SDK 10.0.28000.0. VST3, standalone, tests, diagnostics and benchmark compile. CTest: 2/2 suites pass. Includes Fine Tune selected-range preservation/export, legacy .sts, ignored enabled=0, component space/focus policy, independent G-minor oracle and reset allocations. pluginval strictness 10 with fixed seed 12345 passes on the final bundle (see packaged evidence).

Known-F0 corpus: 36 two-second harmonic chromatic pitches, MIDI 43-78 at alternating +/-30 cents, followed by vibrato/noise/silence stress material. Independent output accuracy is evaluated only in stable held-note interiors (1465 frames), not across the stress material or note transitions:

| Measurement | Observed |
|---|---:|
| Detector median absolute error | 0.243 cents |
| Detector p95 absolute error | 1.982 cents |
| Target-note assignment in evaluated frames | 100% |
| Disabled G-minor target classes | 0 |
| Independent output median absolute error (Quality, Strength 100%, Speed 40 ms) | 1.890 cents |
| Independent output p95 absolute error | 7.892 cents |
| Output gross (>50-cent) / octave error fractions | 0 / 0 |
| Missing output F0 frames in evaluated set | 0 |

Output F0 is measured with separate normalized autocorrelation, not the plugin's YIN. These synthetic results do not establish breathy/consonant handling, formant quality, curated vocal note assignment or Pitcher parity. The separate wide-range detector fixture's worst endpoint error is 7.66 cents. Independent fixed-shifter zero-crossing tests cover 0, +/-1, +/-3, +/-6, +/-12 semitones on both backends; spectral pure-tone results are under 15 cents (this oracle is sensitive to resynthesis residuals).

Quality zero-strength check: aligned PCM24 output versus PCM16 dry corpus, trimming one second at each edge, residual RMS 2.744e-7, peak 8.345e-7 with source RMS 0.10336. This provides a numerical unity reconstruction check, not a vocal identity listening verdict.

## Callback budget and latency

The initial nonsplit spectral configuration missed callback deadlines at 88.2/96 kHz and 32/64-sample buffers. Enabling distributed computation adds 30 ms explicitly to the latency report and reduces work bursts. Current Quality audio delay at 48 kHz is **7200 samples / 150 ms**. Full voice remains 768 / 16 ms; Live 480 / 10 ms (110+ Hz). Fine Tune offline uses its existing 120 ms internal spectral delay, removed during export.

After 0.25 s warmup, three seconds of generated stereo harmonic audio per case, required rates 44.1/48/88.2/96 kHz x buffers 32/64/128/512/1024 x all three current engines: **zero missed deadlines in this run**. This short machine-specific benchmark is not a dropout guarantee or FL soak. Source generation is outside timing. The archive includes both the initial failed unsplit run and final split run.

| Final Quality case | Mean | p95 | Worst | Deadline |
|---|---:|---:|---:|---:|
| 48 kHz / 128 | 45.061 us | 77.100 us | 129.400 us | 2666.667 us |
| 96 kHz / 32 | 9.888 us | 48.300 us | 126.000 us | 333.333 us |

Approximate 48 kHz/128 one-core processing budget: 1.690% Quality. Do not equate this with FL's CPU meter. Detector observation history (~43 ms plus filtering/hop/Speed), shifter delay and device/host round trip remain separate.

## UI and actual host status

Native rendered Auto Tune and Fine Tune snapshots reviewed at 1280x640; green theme and functional editor layout retained. Auto Tune component tree declines keyboard focus and its space handler returns false; Fine Tune handles space. This is component evidence only.

Actual new-build FL keyboard, PDC, MIDI, reopen, export and 30-minute soak are **pending**. FL Studio 2024 was launched in an empty test project, but protected-folder install was denied and computer-use folder-picker errors prevented loading the separate new bundle. Original installed Suitunes was not overwritten. No user song was opened/saved. No Pitcher vocal renders or original dry failure excerpt were available. macOS/AU remains unbuilt here.

The release remains an engineering checkpoint until the [A/B procedure](docs/PITCHER_AB_PROCEDURE.md) and [host checklist](docs/WINDOWS_BUILD_AND_TEST.md) are completed. See [audit](docs/PITCH_POP_AUDIT.md) for remaining algorithm limitations.

---

## Historical 0.2 checkpoint (superseded for current engine defaults)

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
