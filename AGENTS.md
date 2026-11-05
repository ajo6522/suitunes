# Suitunes source of truth

The user designated `docs/reference/Suitunes-V1.0.0-dev-spec.pdf` as the full feature specification on November 4, 2026. Read `docs/SPEC.md` and `docs/IMPLEMENTATION_STATUS.md` before extending the product. Prior linked-chat summaries are superseded.

Visual references: `Assets/reference/Fine-Tune-blue.png` and `Assets/reference/Auto-Tune-layout.png`. Both screens use graphite/black with BLUE/CYAN accents. The user explicitly rejected orange. Keep Speed and Correction Strength prominent at centre, with real rotary controls. Do not return to JUCE's generic parameter editor.

Keep reference PDFs/PNGs unchanged. Build functioning components, not a fake interactive background. No factory presets. Portable `.sts` saves use formatVersion 1 and remain separate from host/editor/audio state. Retain legacy migration fixtures/tests.

Do not claim finished V1 acceptance from synthetic tests/pluginval. Keep missing features and unverified acceptance gates explicit. Distinguish audio delay, detector observation time and monitored round-trip delay. DSP changes require accuracy, variable-buffer, allocation and alignment checks; UI changes require rendered review.
