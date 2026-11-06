# Pitch Pop audio overhaul — November 5, 2026

Review branch: `pitch-pop/audio-overhaul`, based on the clean local `5a2fd66`
repair checkpoint (itself based on remote `fecb24b`). The branch includes that
previously unpushed green-UI/host-compatibility checkpoint. No installed plugin,
synced source material, host identity, preset wire format or user song was changed.

## Implemented

- YIN retains the existing bounded octave-ambiguity guard and onset escape.
  Continued tracking accepts periodicity down to .78; initial acquisition still
  requires .85. Prepare clears all telemetry as well as tracking history.
- Separate measured observations from controller continuity. Carry at most 15 ms
  of missing pitch without voting for a new target, then release correction.
  Onsets, reset, missing MIDI target and an empty custom mask invalidate the hold.
  Strength and scale restrictions remain effective during the hold.
- Correct the target dead band to an actual 15 cents (the former .15 distance
  advantage was only 7.5 cents from a midpoint). Require 60 ms near an ambiguous
  boundary, 20 ms for a clear adjacent transition or 10 ms for a large interval,
  with up to 50 ms extra dwell as confidence falls. Onsets still acquire directly.
- Replace moving audible read-head positions with continuous grain reads.
  Re-anchor at the Hann-window zero using bounded waveform matching, shared by
  stereo channels. Clamp accumulated delay drift to recorded history. Smooth
  transitions to exact unity over 5 ms instead of the former abrupt blend.
  This is pitch-period overlap-add, **not a full epoch-tracked TD-PSOLA engine**.
- Retain the existing Quality/offline Signalsmith formant path and its new-instance
  default. Low-delay formant preservation remains unavailable; no unsupported
  LPC/formant claim or silently changed legacy engine choice was introduced.
- Actual pretrained FCPE and CREPE-tiny inference prototypes, measurements and
  weight hashes. No inference runtime, process or new worker is started by the
  plugin. `NeuralPitchBridge.h` implements a tested bounded SPSC boundary and
  timestamp/epoch/confidence fallback arbitration for a future worker integration;
  it is not connected to the production engine.
- Recorded-vowel fixtures, independent output-pitch checks, confidence/dropout,
  midpoint transition, true-octave acquisition, stereo splice, allocation and
  worker-failure tests. Add 36 processor create/prepare/state/release/destroy
  lifecycles with three simultaneous instances. Add the new suite to Windows CI.
- Fix repeated diagnostic WAV writes: JUCE output streams append by default;
  truncate before writing so repeated experiments produce valid WAV headers.

Fine Tune selection, Select All, wheel zoom, note editing, undo/redo, preview,
exports, legacy STS and keyboard policy remain covered by the existing tests.
Processor/editor thread teardown and host-wrapper code were not rewritten.

## Actual results

Evidence is committed in [evidence/audio-overhaul](evidence/audio-overhaul).

| Check | Observed outcome |
|---|---|
| MSVC x64 Release | VST3, standalone, tests, diagnostics and benchmark built |
| CTest | All 3 suites passed, including 36 processor lifecycles |
| pluginval | Strictness 10, GUI checks, SUCCESS; no FL session exercised |
| Recorded input identity | 671/679 middle-segment frames within 50 cents of audited nominal sung notes; no octave errors |
| Independent rendered output | 18 file/engine combinations: worst median absolute error 18.34 cents; minimum fraction within 50 cents 90.57% |
| Boundary E→G minor case | Median absolute output error 4.20–5.49 cents from E-flat after revised targeting (previous experiment 32–36 cents); fixture-specific pitch result, not a sound-quality judgment |
| Synthetic splice stress | Maximum adjacent-sample step .00880738 on a .2-amplitude swept sine with ratio reversals; stereo identical and C++ allocation guard passed |
| Output delay at 48 kHz | Impulse checks retain Live 480 samples (10 ms), Full voice 768 (16 ms), Quality 7200 (150 ms); spectral peak checked within 128 samples |
| CPU matrix | 4 rates × 5 buffer sizes × 3 engines, stereo: mean 0.656–3.003% of one core; **3 deadline misses** in the final run, all at 44.1 kHz/32 samples (2 Live, 1 Quality) |

These short offline timings do not establish a real-time deadline guarantee.
The legacy `baseline` benchmark row is a different spectral configuration and
must not be used as a like-for-like speedup claim. Detector history (~43 ms),
hop/FIR delay, retune, transformed transient timing and hardware round trip are
distinct from the impulse/PDC figures.

## Actual neural experiments

CPU, one Torch thread, installed `torchfcpe==0.0.4`, `torchcrepe==0.0.24`,
PyTorch/Torchaudio 2.6.0 CPU; exact environment and model SHA256 are recorded.
Five exact-frequency harmonic tones, silence, deterministic white noise and
six one-second excerpts of recorded vowels were evaluated.

| Prototype | Measured behavior |
|---|---|
| CREPE-tiny trailing window | 64 ms observation, frame centre 32 ms behind available input; independent weighted-argmax decoding without sequence Viterbi. Median inference per case 2.30–2.44 ms, worst case p95 2.81 ms; 0/1222 calls exceeded 10 ms |
| FCPE bounded-context experiment | 200 ms trailing context; evaluate 20 ms before its right edge. Median inference 10.22–10.80 ms, worst case p95 11.63 ms; 876/1053 calls exceeded 10 ms |
| Transition response | 220→330 Hz, phase continuous: third consecutive correct result available after 56.29 ms (tiny), 71.05 ms (FCPE); includes measured call time, excludes retune/shifter/OS queueing/device delay |
| Unvoiced rejection | Silence rejected. At the tested thresholds, white-noise false-voicing was 74.47% tiny / 72.84% bounded FCPE / 93.07% offline FCPE |

Both prototypes recognized the nominal corrected-octave sung notes. The raw
JSON includes per-frame arrivals, F0, nominal-reference accuracy, wall timing
and process CPU timing (the latter is coarse on Windows). CREPE-tiny is a causal
**wrapper** using only a trailing frame, not a newly trained causal network.
FCPE remains a context-trained noncausal architecture even when its input is
truncated. Its full-file inference is also measured for offline Fine Tune research.

**Decision:** do not ship either experiment as the default live detector.
Confidence is not yet calibrated; the noise failures and FCPE deadline failures
are material. A future optional worker must use preallocated trailing frames,
bounded queues that drop rather than wait, sample timestamps and transport
epochs. Reject future/stale/low-confidence results; keep deterministic YIN when
the model stalls, crashes or disagrees with reliable periodic evidence. Never
load models, allocate tensors, wait for inference or join workers in the callback.
The bridge tests cover queue overflow, missing/stale results and old epochs.
Long-context models and potential neural resynthesis remain offline Fine Tune
research; no neural resynthesizer or trained new causal model is claimed.

## Reproduction

Use the normal Windows build instructions, adding `PitchPopOverhaul` to targets,
then run CTest. The Python experiments use an isolated environment under build:

```powershell
python -m venv build/neural-env
build/neural-env/Scripts/python -m pip install torch==2.6.0 torchaudio==2.6.0 --index-url https://download.pytorch.org/whl/cpu
build/neural-env/Scripts/python -m pip install -r docs/evidence/audio-overhaul/neural-requirements-lock.txt
build/neural-env/Scripts/python tools/NeuralPitchBench.py build/evidence/neural-pitch.json
build/neural-env/Scripts/python tools/VocalOutputBench.py build/PitchPopDiagnostics_artefacts/Release/PitchPopDiagnostics.exe build/evidence/vocal-output
```

Model loading/warmup and input resampling are outside the reported inference
times. Results represent isolated sequential calls, not a running device pipeline.
The output benchmark checks duration plus independent autocorrelation on middle
segments and fails on median error >25 cents or <85% within 50 cents. These
corpus-specific thresholds are not release acceptance thresholds.

## Outstanding release gates

The requested complete production overhaul is **not accepted as finished**.
No user problem vocal/settings or same-input Pitcher render was available.
No blind listening, lyric/consonant/breathy corpus, calibrated neural confidence,
trained causal model, full TD-PSOLA/low-delay formant engine, neural runtime
integration or neural resynthesis is delivered. Natural-vocal formant quality
and audible-pop elimination remain unproven. A full G-minor mapper oracle and
these vowels do not prove the absence of wrong notes on arbitrary singing.

FL Studio shutdown, stuck mixer instances, interaction with unrelated plugins,
spacebar forwarding, PDC/looping and a 30-minute soak still require an actual
FL session with this build. Passing pluginval and repeated isolated processor
lifecycles are useful regressions, not substitutes. Existing installed bundles
were left intact. macOS/AU remain unvalidated.
