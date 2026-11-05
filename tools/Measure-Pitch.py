"""Offline reference fixtures and independent output-F0 measurement (Python + NumPy).

generate: deterministic held harmonic chromatic pitches, vibrato, noise and silence.
measure: autocorrelation F0 with parabolic interpolation, without the plugin's YIN.
Metrics exclude attack/release and transition intervals explicitly; not vocal acceptance.
"""
import argparse
import csv
import json
from pathlib import Path
import wave
import numpy as np


def read_wav(path):
    with wave.open(str(path), "rb") as file:
        rate, channels, width = file.getframerate(), file.getnchannels(), file.getsampwidth()
        raw = file.readframes(file.getnframes())
    if width == 2:
        values = np.frombuffer(raw, "<i2").astype(float) / 32768
    elif width == 3:
        b = np.frombuffer(raw, np.uint8).reshape(-1, 3).astype(np.int32)
        values = b[:, 0] | (b[:, 1] << 8) | (b[:, 2] << 16)
        values = ((values ^ 0x800000) - 0x800000) / 8388608
    else:
        raise ValueError("Export PCM 16/24-bit WAV for independent measurement")
    # Choose the strongest channel; do not cancel antiphase stereo.
    values = values.reshape(-1, channels)
    return rate, values[:, np.argmax(np.mean(values**2, axis=0))]


def estimate(frame, rate):
    frame = frame - np.mean(frame)
    if np.sqrt(np.mean(frame**2)) < .0005:
        return 0., 0.
    n = len(frame)
    # Unwindowed normalized autocorrelation, overlap-energy compensation.
    fft = np.fft.rfft(frame, 2*n)
    corr = np.fft.irfft(fft*np.conj(fft), 2*n)[:n]
    energy = np.r_[0., np.cumsum(frame*frame)]
    lag = np.arange(1, n//2)
    corr[lag] /= np.maximum(1e-15, np.sqrt(energy[n-lag]*(energy[n]-energy[lag])))
    lo, hi = max(2, int(rate/1000)), min(n//2-2, int(rate/65))
    peaks = [i for i in range(lo, hi+1) if corr[i] >= corr[i-1] and corr[i] > corr[i+1]]
    if not peaks:
        return 0., 0.
    best = max(corr[i] for i in peaks)
    if best < .8:
        return 0., float(best)
    # Earliest strong periodic peak avoids selecting repeated-period subharmonics.
    i = next(i for i in peaks if corr[i] >= max(.8, best-.03))
    a, b, c = corr[i-1:i+2]
    den = a-2*b+c
    offset = np.clip(.5*(a-c)/den, -.5, .5) if abs(den) > 1e-12 else 0.
    return float(rate/(i+offset)), float(b)


def generate(folder):
    folder.mkdir(parents=True, exist_ok=True)
    rate, duration = 48000, 2.
    segments, annotations = [], []
    # G-minor reference mapping is independent of the C++ implementation.
    allowed = {0, 2, 3, 5, 7, 9, 10}
    for note in range(43, 79):
        cents = 30. if note % 2 else -30.
        midi = note+cents/100
        target = min((n for n in range(note-12, note+13) if n % 12 in allowed), key=lambda n: (abs(n-midi), n))
        t = np.arange(int(rate*duration))/rate
        hz = 440*2**((midi-69)/12)
        phase = 2*np.pi*hz*t
        envelope = np.minimum(1, np.minimum(t/.02, (duration-t)/.02))
        audio = .12*envelope*(np.sin(phase)+.6*np.sin(2*phase)+.4*np.sin(3*phase))
        start = len(segments)*duration
        annotations.append(dict(start=start, end=start+duration, source_hz=hz, target_hz=440*2**((target-69)/12), type="held-harmonic"))
        segments.append(audio)
    # Separate stress material; no guessed labels or accuracy claims for these.
    t = np.arange(rate*4)/rate
    freq = 220*2**(.35*np.sin(2*np.pi*5*t)/12)
    phase = 2*np.pi*np.cumsum(freq)/rate
    stress = .15*np.sin(phase)
    noise = np.random.default_rng(20261105).uniform(-.04, .04, rate)
    audio = np.r_[np.concatenate(segments), stress, noise, np.zeros(rate)]
    with wave.open(str(folder/"chromatic-g-minor.wav"), "wb") as file:
        file.setparams((1, 2, rate, 0, "NONE", "not compressed"))
        file.writeframes((np.clip(audio, -1, 1)*32767).astype("<i2").tobytes())
    (folder/"reference.json").write_text(json.dumps(dict(rate=rate, segments=annotations), indent=2))


def measure(wav, reference, output):
    rate, audio = read_wav(wav)
    refs = json.loads(reference.read_text())["segments"]
    output.mkdir(parents=True, exist_ok=True)
    errors, gross, octaves, missing, rows = [], 0, 0, 0, []
    for ref in refs:
        # Central 0.8 s of each 2 s held note: avoid labelling transitions stable.
        for time in np.arange(ref["start"]+.8, ref["end"]-.4, .02):
            centre = int(time*rate)
            width = int(.08*rate)//2
            hz, confidence = estimate(audio[centre-width:centre+width], rate)
            cents = 1200*np.log2(hz/ref["target_hz"]) if hz else None
            rows.append((time, hz, confidence, ref["target_hz"], cents))
            if cents is None:
                missing += 1
            else:
                errors.append(abs(cents));gross += abs(cents)>50;octaves += abs(abs(cents)-1200)<100
    with (output/"output-f0.csv").open("w", newline="") as file:
        writer = csv.writer(file);writer.writerow(("seconds", "output_hz", "periodicity", "expected_hz", "error_cents"));writer.writerows(rows)
    result = dict(file=str(wav), oracle="independent normalized autocorrelation", evaluated_frames=len(rows), missing_frames=missing,
                  median_cents=float(np.median(errors)) if errors else None, p95_cents=float(np.percentile(errors, 95)) if errors else None,
                  gross_error_fraction=gross/len(rows), octave_error_fraction=octaves/len(rows),
                  limitation="Steady synthetic harmonic signals only; not vocal, formant, transient or Pitcher acceptance")
    trace_path = output/"trace.csv"
    if trace_path.exists():
        detection, targets, disabled = [], [], 0
        with trace_path.open() as file:
            for row in csv.DictReader(file):
                if None in row or any(value is None for value in row.values()):
                    raise ValueError("Malformed diagnostic CSV row")
                time = int(row["sample_position"])/rate
                ref = next((s for s in refs if s["start"]+.8 <= time <= s["end"]-.4), None)
                if ref is None:
                    continue
                hz = float(row["chosen_f0_hz"])
                detection.append(abs(1200*np.log2(hz/ref["source_hz"])) if hz else 9999.)
                target = float(row["target_midi"])
                expected = 69+12*np.log2(ref["target_hz"]/440)
                targets.append(abs(target-expected)<.01)
                disabled += round(target) % 12 not in {0, 2, 3, 5, 7, 9, 10}
        if detection:
            result.update(detector_median_cents=float(np.median(detection)), detector_p95_cents=float(np.percentile(detection, 95)),
                          detector_gross_error_fraction=float(np.mean(np.array(detection)>50)),
                          target_assignment_fraction=float(np.mean(targets)), out_of_scale_targets=disabled)
    (output/"metrics.json").write_text(json.dumps(result, indent=2));print(json.dumps(result, indent=2))


if __name__ == "__main__":
    parser = argparse.ArgumentParser(); sub = parser.add_subparsers(dest="command", required=True)
    create = sub.add_parser("generate");create.add_argument("folder", type=Path)
    run = sub.add_parser("measure");run.add_argument("wav", type=Path);run.add_argument("reference", type=Path);run.add_argument("output", type=Path)
    args = parser.parse_args()
    if args.command == "generate":generate(args.folder)
    else:measure(args.wav, args.reference, args.output)
