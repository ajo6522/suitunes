# Recorded sustained vowels

Six human sung /a/ notes from vocobox/human-voice-dataset, pinned source paths
and SHA256 in provenance.json, original MIT license in LICENSE.txt. Copied from
the existing local Layer Cake fixtures; recordings are unchanged.

**Do not use filenames as octave truth.** A3.wav has fundamental/harmonic spacing
about 109 Hz (A2); B3.wav about 122 Hz (B2). C3/D3/E3/G3 have fundamentals around
131/146/165/196 Hz. Independently checked using the harmonic spacing in a Hann
window FFT of the middle half-second, alongside FCPE, CREPE-tiny and YIN results.
The A/B octave discrepancy exists in the recordings/filenames; it is not fixed
by changing the detector. Tests use MIDI 48,50,52,55,45,47 respectively.

These are nominal note-identity references, not framewise ground-truth F0. One
male singer, sustained vowels only: no lyric/consonant/breathy/general-vocal
acceptance or listening-quality claim can be made from this corpus.
