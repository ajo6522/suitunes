# Suitunes source of truth

## Pitch Pop update - November 5, 2026

The current user's explicit requirements supersede older naming/colour/toggle instructions below: product is Pitch Pop, green soda branding, Auto Tune correction always enabled, no Correction On/Off UI. Preserve Fine Tune selection/Select All/wheel zoom, editing and .sts compatibility. Auto Tune must not consume space; Fine Tune may use it for preview. Verify in FL Studio before claiming host acceptance.

`docs/PITCH_POP_AUDIT.md` records the new research review, source baseline and remaining gates. The November 5 research PDF is engineering reference data, not proof of performance or permission to replace working features. Quality defaults to spectral/formants for new instances; existing explicit engine choices migrate unchanged. Preserve host IDs and STS wire format during this rename.

The user designated `docs/reference/Suitunes-V1.0.0-dev-spec.pdf` as the full feature specification on November 4, 2026. Read `docs/SPEC.md` and `docs/IMPLEMENTATION_STATUS.md` before extending the product. Prior linked-chat summaries are superseded.

Visual references: `Assets/reference/Fine-Tune-blue.png` and `Assets/reference/Auto-Tune-layout.png`. Both screens use graphite/black with BLUE/CYAN accents. The user explicitly rejected orange. Keep Speed and Correction Strength prominent at centre, with real rotary controls. Do not return to JUCE's generic parameter editor.

Keep reference PDFs/PNGs unchanged. Build functioning components, not a fake interactive background. No factory presets. Portable `.sts` saves use formatVersion 1 and remain separate from host/editor/audio state. Retain legacy migration fixtures/tests.

Do not claim finished V1 acceptance from synthetic tests/pluginval. Keep missing features and unverified acceptance gates explicit. Distinguish audio delay, detector observation time and monitored round-trip delay. DSP changes require accuracy, variable-buffer, allocation and alignment checks; UI changes require rendered review.
