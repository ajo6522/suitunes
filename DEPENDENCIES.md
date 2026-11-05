# Dependency record

- JUCE: 8.0.8 tag, fetched by CMake (AGPLv3/commercial). Optional existing source override. JUCE provides its embedded VST3 SDK and associated notices.
- Signalsmith Stretch: vendored from `https://github.com/Signalsmith-Audio/signalsmith-stretch`, commit `88061fd4eda56a2353c5d65c74aa3f4ccf76f9da`, retrieved November 4, 2026. MIT license in `vendor/signalsmith-stretch/LICENSE.txt`.
- Signalsmith Linear: 0.6.6 tag fetched by upstream Stretch's CMake. MIT license in the fetched source. No optional accelerated FFT backend enabled.

Dependency upgrades must run the detector, dry alignment, correction and host validation checks again. Do not silently change the stretch configuration to hit a latency marketing target.

The authored Suitunes source has no additional open-source license grant in this prototype delivery. Third-party licenses remain applicable. This is not a signed production release.
