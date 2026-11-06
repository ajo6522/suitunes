# Audio overhaul source and license review

Reviewed November 5, 2026. No Flayr Tune or SnapTune source was copied, translated
or vendored. Shared general techniques—periodicity confidence, octave continuity,
target hysteresis, pitch-period overlap-add and phase-compatible splices—were
implemented directly against this repository's existing engine.

| Material | Source/license | Decision |
|---|---|---|
| Flayr Tune | [Upstream](https://github.com/FlayrLabs/flayr-tune), AGPL-3.0 | Conceptual reference only; no code/assets/dependencies imported |
| SnapTune | [Upstream](https://github.com/NicoFerni/snaptune), AGPL-3.0 | Conceptual reference only; do not assume permissive licensing |
| FCPE | [Upstream license](https://github.com/CNChTu/FCPE/blob/main/LICENSE), MIT, CN_ChiTu 2023 | Installed 0.0.4 for external offline experiment only; bundled fcpe_c_v001.pt hash recorded. License retained with evidence. No runtime/weights shipped in VST3 |
| torchcrepe | [Upstream license](https://github.com/maxrmorrison/torchcrepe/blob/master/LICENSE), MIT, Max Morrison 2020 | Installed 0.0.24 externally; tiny weights actually invoked. Full weights included in the package but not inferred. License and hashes retained; no runtime/weights shipped in VST3 |
| Human singing | [Pinned upstream](https://github.com/vocobox/human-voice-dataset/tree/77248fc69fd93c40a69d49c0cade4144c5d7a9f4), MIT, vocobox 2014 | Six unchanged WAVs reused with license, original provenance/hashes and independently corrected octave references |
| Signalsmith Stretch | Existing vendored MIT license, Geraint Luff / Signalsmith Audio | Retained existing DSP dependency and notices; no additional third-party C++ DSP source reused |
| JUCE 8.0.8 | Existing AGPL/commercial dual-license dependency | Proprietary distribution still requires an applicable commercial JUCE license; this branch does not grant one |

MIT code availability does not establish a vocal-quality, latency or training-data
guarantee. FCPE/CREPE experiments rely on upstream-distributed weights under the
repositories' published licenses; no separately audited training-data rights
claim is made. A future redistributed neural runtime must retain the weight/code
licenses and audit all packaged transitive/native dependencies, not just the top
level model package. The experiment environment lock is evidence, not a VST3
shipping manifest. No GPL/AGPL pitch-correction code was added to Pitch Pop.
