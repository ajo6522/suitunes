# Resume here — November 5, 2026

## Actual listening result

The user tested the current prototype and reported **warbling and wrong notes**. The correction engine has failed the real-vocal quality requirement despite passing synthetic tests and pluginval. Do not describe this checkpoint as acceptable vocal correction, production-ready, or equivalent to Pitcher.

The exact engine mode, input recording, key/scale and settings from that test have not been captured. A dry excerpt and, preferably, Pitcher output/settings on the same excerpt are still needed to reproduce the failure reliably.

## Required sound

Automatic, full-strength correction that sounds smooth and natural like FL Studio Pitcher, without warbling, incorrect note/octave changes or robotic transitions. Reducing Strength to hide artifacts does not meet the requirement. Prioritize vocal quality and stable tracking before further latency/CPU optimization or interface work.

Image-Line's public Pitcher manual documents controls and behavior, not the internal algorithm:
https://www.image-line.com/fl-studio-learning-content/fl-studio-online-manual/html/plugins/Pitcher.htm

Use it as a behavioral reference: allowed-note targeting, minimum detection frequency, correction-speed behavior, channel reference and formant handling. Identical sound has not been established and cannot be promised from the manual alone.

## Next investigation

1. Reproduce the user's failure on the actual dry vocal with recorded settings.
2. Capture source F0/confidence, selected target, octave changes and requested/applied ratio. Separate detector/target instability from shifter artifacts.
3. Evaluate the custom pitch-synchronous shifter and spectral engine independently at fixed known shifts on the same vocal. The short-delay engine remains an experiment, not the accepted final design.
4. Improve note continuity/voicing/octave handling and correction trajectories against real singing, slides, vibrato and consonants. Evaluate a replacement shifter if needed.
5. Compare against Pitcher on the same material, loudness-matched, at full correction. Require listening acceptance in addition to numerical accuracy, delay, CPU and allocation checks.

## Saved checkpoint

Source: prototype 0.2.0, C++17/JUCE 8.0.8. Windows x64 VST3 and standalone compiled locally; automated suite and pluginval strictness 10 passed before this save. Reported profiles: Full voice ~16 ms, Live 10 ms for input >=110 Hz, Spectral 120 ms. These measurements do not establish satisfactory vocal quality.

The user-supplied PDF in `docs/reference/` governs feature behavior. The supplied PNGs in `Assets/reference/` govern the UI, with blue/cyan instead of orange. Keep the current UI while fixing the engine. `IMPLEMENTATION_STATUS.md` lists all remaining V1 gaps, including low-delay formants and unverified host/macOS acceptance.

No DSP fixes were attempted after the user's request to save the checkpoint for tomorrow. Build products, temporary audio, local caches and development-machine logs are intentionally excluded from Git.
