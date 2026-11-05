# Pitch Pop 0.3 engineering checkpoint - November 5, 2026

## Authority and preserved baseline

Reviewed all 11 pages of `reference/Suitunes_Pitch_Correction_Research_Specification.pdf`. It expressly describes research/proposed acceptance targets, not an audited V1 or demonstrated Pitcher parity. The user's current request controls scope: Pitch Pop name and green soda theme, always-enabled Auto Tune, FL transport spacebar, preserved Fine Tune editing/selection/Select All/wheel zoom and `.sts`. Earlier blue branding and correction-switch instructions are superseded. Reference PDFs/PNGs and synced `sources/` are unchanged.

Checked the actual GitHub remote: `https://github.com/ajo6522/suitunes.git` HEAD and the clean local checkout both pointed to `fecb24ba17f248cdd8a3dfd8ad52bab3f14b7cc7`. It now contains the JUCE source. The research's earlier statement that code was not pushed is historical. Baseline tag: `pitch-pop-pre-repair-2026-11-05`. Work branch: `pitch-pop/engine-repair`. No remote push or overwrite of the installed Suitunes bundle was performed. A copy of the installed bundle was preserved under `build/pre-install-backup/`.

## Audit and implemented changes

| Path | Finding and action |
|---|---|
| Detection | Original filtered/decimated YIN accepted the first threshold minimum. Retain up to 16 local candidates, RMS gate and persistent short octave-ambiguity hold (maximum eight detector hops). A rising-energy onset clears the continuity preference. Genuine interval acquisition remains bounded. This is an original deterministic tracker, not full pYIN or harmonic-model inference. |
| Musical mapping | Original C=0 natural-minor mask/root transposition and signed modulo were correct. Moved mapping and target state into JUCE-independent `PitchControl.h`; added exhaustive independent G-minor oracle, negative notes, octave boundaries and lower-note midpoint ties. G minor uses C,D,Eb,F,G,A,Bb (mask 1709). |
| Target/trajectory | Original control updates were coupled to partial 64-sample chunks. Decisions and centre filters now update on fixed detector hops, with sample-wise retune smoothing. Added 15-cent hysteresis and 20 ms adjacent-note dwell; large transitions use 10 ms, onsets acquire immediately. Controller trajectory and target are checked against 1-sample versus 1024-sample host callbacks. Audio resynthesis itself is not claimed bit-identical under arbitrary segmentation. |
| Shift sign/strength | Positive semitones multiply frequency by `2^(semitones/12)`. Strength scales intended semitone correction; it is not implemented as wet/dry blend. Independent zero-crossing tests cover 0, +/-1, +/-3, +/-6 and +/-12 semitones on both backends. |
| Formants/resynthesis | The two-read-head low-delay shifter is an experiment without vocal-tract envelope preservation. Its moving periods can modulate nonstationary vocals; synthetic pitch accuracy does not clear this risk. New instances now default to the existing Signalsmith spectral/formant backend, labelled Quality. Existing mode values are retained. Both experimental engines remain selectable. Low-delay formants were not invented or claimed. |
| Voiced/unvoiced | RMS/periodicity gates reject silence and deterministic white noise, reset target state, and relax correction toward unity with Speed. RMS onset is a limited proxy. Spectral flatness, robust consonant segmentation, calibrated breathy-vocal confidence and natural-vocal octave validation remain open. |
| Latency | Retain measured/reported 16 ms Full voice, 10 ms Live (110+ Hz), 150 ms Quality audio delay at 48 kHz. Quality is not a live-monitoring 10 ms solution. Detector history (~43 ms plus FIR/hop and Speed) and hardware/FL round trip are separate quantities. |
| Transport | Stop/start/seek clears detector, target, MIDI held notes and prepared audio buffers without reallocation. Allocation guards cover all mode resets; reported latency stays constant. Actual FL loop/seek and PDC acceptance remain pending. |
| UI/space | Removed the Correction button and attachment. Auto Tune recursively declines mouse-acquired keyboard focus; space handler returns false. Auto Tune value displays are read-only to prevent dynamically-created numeric editors from capturing transport; Fine Tune retains numeric entry. Fine Tune handles space for preview. CMake explicitly declines wrapper keyboard-focus preference. Component policy tests pass; host forwarding is not thereby proven. |
| Telemetry | Target display now reports the selected note instead of detected pitch plus a smoothed correction amount. CSV diagnostics are offline only; the audio callback does not format/write traces. Existing atomic UI telemetry remains bounded. |
| Presets/identity | `.sts`, formatVersion 1, legacy `SUITUNES_STS`/`SuitunesPreset`, state root, parameter ordering and VST3 identity remain stable. New preset metadata says Pitch Pop 0.3. Existing `enabled` parameter stays as a deprecated, nonautomatable, ignored ID; legacy enabled=0 cannot turn correction off. Host bypass and Strength still operate. |
| Fine Tune | Shared detector improvements apply to analysis. Selection splits, Select All, wheel zoom, note edits, undo/redo, original/corrected preview, WAV/MIDI exports and source references are preserved. Offline formant processing and bit-exact samples outside edited ranges remain tested. |

## Callback burst fix

The initial unsplit Quality configuration had 120 ms audio delay but missed deadlines at 88.2/96 kHz with 32/64-sample callbacks. Enabled Signalsmith's distributed computation while retaining its spectral window/hop. The extra 30 ms is reported by `outputLatency()` and JUCE, for 150 ms at 48 kHz. The expanded post-fix benchmark has zero current-engine deadline misses in the tested matrix. This does not establish monitoring suitability or a host soak. Offline Fine Tune remains nonsplit at 120 ms internally, compensated during export.

## Host attempt and open gates

Windows x64 VST3 and standalone compile; strictness-10 pluginval succeeds. Component keyboard checks and native rendered UI reviews pass. FL Studio 2024 was launched in a new empty project; no user song was opened or saved. Protected-folder installation was denied; the original installed plugin is untouched. Attempting to add the separate build folder in FL Plugin Manager hit computer-use errors: `point ... is over PluginManager.exe "", not target window ...` and then `element 155 is not available in cached app state`. Therefore **the new build was not loaded into FL and spacebar/PDC/host acceptance remain unverified**. Do not describe this attempt as a passed host test.

The reported warbling/wrong-note vocal is not attached. A dry excerpt, original settings, and same-input Pitcher render are needed for reproduction and blind listening. No natural-vocal accuracy, formant-envelope metric, proprietary parity, 30-minute FL soak or macOS acceptance is claimed. These are outstanding release gates, not reasons to weaken Strength and hide artifacts.

See `WINDOWS_BUILD_AND_TEST.md`, `PITCHER_AB_PROCEDURE.md` and `../VALIDATION.md` for reproducible checks and measured evidence.
