"""Actual pretrained FCPE and causal-window CREPE-tiny CPU experiments.

Offline only. Never imported/launched by the plugin. Frame arrival timestamps
include the observation window and measured inference, not just network runtime.
CREPE uses independent weighted-argmax frames, never full-sequence Viterbi.
FCPE uses native full context for offline analysis and a bounded trailing-context
experiment. Truncating its context does NOT make its trained network causal.
"""
import argparse
import hashlib
import importlib.metadata
import json
import platform
import time
from pathlib import Path
import numpy as np
import soundfile as sf
import torch
import torchaudio
import torchcrepe
from torchfcpe import spawn_bundled_infer_model


def stats(values):
    a = np.asarray(values, dtype=float)
    return {"median": float(np.median(a)), "p95": float(np.percentile(a, 95)), "max": float(a.max())}


def accuracy(predictions, truth):
    p = np.asarray(predictions)
    valid = np.isfinite(p) & (p > 0)
    if truth == 0:
        return {"frames":len(p),"false_voiced_fraction":float(valid.mean())}
    cents = np.abs(1200 * np.log2(p[valid] / truth))
    return {"frames": len(p), "voiced_fraction": float(valid.mean()),
            "median_absolute_cents": float(np.median(cents)) if len(cents) else None,
            "within_50_cents_fraction_all_frames": float(np.sum(cents < 50) / len(p)),
            "octave_error_fraction_all_frames": float(np.sum((cents > 1100) & (cents < 1300)) / len(p))}


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("output", type=Path)
    parser.add_argument("--fixtures", type=Path, default=Path("Tests/fixtures/singing"))
    args = parser.parse_args()
    torch.set_num_threads(1)
    torch.set_num_interop_threads(1)
    torch.manual_seed(123)
    fcpe = spawn_bundled_infer_model(device="cpu")
    sr, hop, window = 16000, 160, 1024
    fcpe_window = 3200  # 200 ms observation, evaluated 20 ms before right edge.
    fcpe_hop = fcpe.get_hop_size_ms() / 1000

    def tiny(x):
        p, c = torchcrepe.predict(x[None], sr, hop_length=hop, model="tiny",
                                 fmin=65, fmax=1000, decoder=torchcrepe.decode.weighted_argmax,
                                 return_periodicity=True, pad=False)
        return float(p[0, -1]) if float(c[0, -1]) >= .1 else 0.

    def bounded_fcpe(x):
        p = fcpe.infer(x[None], sr, interp_uv=False)
        return float(p[0, -1-int(round(.020/fcpe_hop)), 0])

    cases = []
    for hz in (82.4069, 130.8128, 220., 440., 880.):
        t = torch.arange(sr) / sr
        x = .15 * (torch.sin(2*torch.pi*hz*t) + .6*torch.sin(4*torch.pi*hz*t))
        cases.append((f"synthetic-{hz}", x, hz, "exact oscillator frequency"))
    cases.append(("silence",torch.zeros(sr),0,"unvoiced"))
    cases.append(("white-noise",.05*torch.randn(sr),0,"unvoiced"))
    for name, midi in (("C3",48),("D3",50),("E3",52),("G3",55),("A3",45),("B3",47)):
        data, rate = sf.read(args.fixtures / f"{name}.wav", always_2d=True, dtype="float32")
        x = torchaudio.functional.resample(torch.from_numpy(data.mean(axis=1)), rate, sr)
        # Sustained middle segment; nominal note labels aren't framewise F0 truth.
        start = max(0, (len(x)-sr)//2)
        cases.append((f"sung-{name}", x[start:start+sr], 440*2**((midi-69)/12), "nominal sung note, not exact F0"))
    results = []
    for name, audio, truth, reference in cases:
        for label, fn, context, age in (("crepe-tiny-trailing",tiny,window,.032),
                                        ("fcpe-trailing-experiment",bounded_fcpe,fcpe_window,.020)):
            fn(audio[:context])  # warmup/loading excluded, explicitly.
            pitches, times, cpu_times = [], [], []
            arrivals = []
            for end in range(context, len(audio)+1, hop):
                cpu_start, start = time.process_time(), time.perf_counter()
                pitches.append(fn(audio[end-context:end]))
                elapsed = time.perf_counter()-start
                times.append(elapsed*1000)
                cpu_times.append(time.process_time()-cpu_start)
                arrivals.append({"available_input_seconds": end/sr,
                                 "pitch_timestamp_seconds": end/sr-age,
                                 "result_available_seconds": end/sr+elapsed})
            result = {"case": name, "model": label, "reference": reference,
                      "history_ms": context/sr*1000, "timestamp_age_ms": age*1000,
                      "compute_ms": stats(times), "timestamp_to_result_ms": stats(np.asarray(times)+age*1000),
                      "single_core_cpu_percent_at_10ms_hops": sum(cpu_times)/(len(times)*.010)*100,
                      "missed_10ms_deadlines": int(np.sum(np.asarray(times)>10)),
                      "accuracy": accuracy(pitches,truth), "arrivals": arrivals, "hz": pitches}
            results.append(result)
            print(name,label,result["accuracy"],result["compute_ms"],flush=True)
        start=time.perf_counter()
        full=fcpe.infer(audio[None],sr,interp_uv=False).flatten().numpy()
        results.append({"case":name,"model":"fcpe-offline","reference":reference,
                        "whole_file_compute_ms":(time.perf_counter()-start)*1000,
                        "accuracy":accuracy(full,truth)})
    assets = {}
    for package in ("torchfcpe","torchcrepe"):
        module=__import__(package)
        for asset in Path(module.__file__).parent.rglob("*"):
            if asset.suffix in (".pt",".pth"):
                assets[f"{package}/{asset.name}"]=hashlib.sha256(asset.read_bytes()).hexdigest()
    responses=[]
    t=torch.arange(sr)/sr
    phase=2*torch.pi*(220*torch.minimum(t,torch.tensor(.5))+330*torch.clamp(t-.5,min=0))
    transition=.15*torch.sin(phase)
    for label,fn,context in (("crepe-tiny-trailing",tiny,window),("fcpe-trailing-experiment",bounded_fcpe,fcpe_window)):
        consecutive=0;first=None;observations=[]
        for end in range(context,sr+1,hop):
            start=time.perf_counter();hz=fn(transition[end-context:end]);elapsed=time.perf_counter()-start
            available=end/sr+elapsed
            correct=end/sr>=.5 and hz>0 and abs(1200*np.log2(hz/330))<50
            consecutive=consecutive+1 if correct else 0
            observations.append({"input_available_seconds":end/sr,"result_available_seconds":available,"hz":hz})
            if consecutive==3 and first is None:first=available-.5
        responses.append({"model":label,"transition_220_to_330_hz_seconds":.5,
                          "three_consecutive_correct_results_after_transition_ms":None if first is None else first*1000,
                          "observations":observations})
    args.output.parent.mkdir(parents=True,exist_ok=True)
    args.output.write_text(json.dumps({"platform":platform.platform(),"processor":platform.processor(),
        "torch_threads":1,"versions":{p:importlib.metadata.version(p) for p in ("torch","torchaudio","torchcrepe","torchfcpe","numpy")},
        "weights_sha256":assets,"results":results,"transition_response":responses,
        "timing_scope":"Isolated calls on CPU; logical sample arrival plus measured inference, excludes OS queueing, retune, synthesis and hardware. Not monitored round-trip latency."},indent=2),encoding="utf8")


if __name__ == "__main__":
    main()
