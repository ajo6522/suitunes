"""Render recorded vowels and measure output with independent autocorrelation.
No subjective quality inference is made from pitch/adjacent-sample metrics.
"""
import argparse
import json
import subprocess
from pathlib import Path
import numpy as np
import soundfile as sf
from scipy.signal import correlate, find_peaks


def f0(frame, sr):
    frame=frame-frame.mean()
    c=correlate(frame,frame,mode="full",method="fft")[len(frame)-1:]
    # Unbiased normalized correlation; search first strong positive periodicity.
    c=c/np.maximum(1,np.arange(len(frame),0,-1))
    c=c/max(c[0],1e-15)
    low,high=int(sr/1000),min(len(c)-2,int(sr/65))
    peaks,_=find_peaks(c[low:high+1])
    peaks=peaks+low
    peaks=[i for i in peaks if c[i]>.8]
    if not peaks:return 0.
    k=peaks[0]
    d=c[k-1]-2*c[k]+c[k+1]
    return sr/(k+(float(np.clip(.5*(c[k-1]-c[k+1])/d,-.5,.5)) if abs(d)>1e-9 else 0))


def main():
    p=argparse.ArgumentParser(description=__doc__)
    p.add_argument("diagnostics",type=Path)
    p.add_argument("output",type=Path)
    args=p.parse_args()
    args.output.mkdir(parents=True,exist_ok=True)
    rows=[]
    # G natural minor oracle, independently written pitch classes.
    allowed={0,2,3,5,7,9,10}
    for name,note in (("C3",48),("D3",50),("E3",52),("G3",55),("A3",45),("B3",47)):
        source=Path("Tests/fixtures/singing")/f"{name}.wav"
        target=min((n for n in range(note-2,note+3) if n%12 in allowed),key=lambda n:(abs(n-note),n))
        for mode in (0,1,2):
            directory=(args.output/f"{name}-{mode}").resolve()
            subprocess.run([str(args.diagnostics.resolve()),str(source.resolve()),str(directory),str(mode),"40","1","1"],check=True,capture_output=True)
            audio,sr=sf.read(directory/"corrected.wav",always_2d=True)
            if len(audio)!=sf.info(source).frames:
                raise RuntimeError("Aligned render changed source duration")
            x=audio.mean(axis=1)
            n=int(sr*.060)
            estimates=[]
            for start in range(len(x)//4,len(x)*3//4-n,int(sr*.010)):
                hz=f0(x[start:start+n],sr)
                estimates.append(69+12*np.log2(hz/440) if hz>0 else np.nan)
            errors=np.abs(np.asarray(estimates)-target)*100
            rows.append({"file":name,"mode":mode,"nominal_input_midi":note,"expected_g_minor_midi":target,
                         "median_output_midi":float(np.nanmedian(estimates)),
                         "median_absolute_cents_from_target":float(np.nanmedian(errors)),
                         "within_50_cents_fraction":float(np.mean(errors<50)),
                         "maximum_adjacent_sample_step":float(np.max(np.abs(np.diff(x)))),
                         "rms":float(np.sqrt(np.mean(x*x)))})
            print(rows[-1],flush=True)
    (args.output/"metrics.json").write_text(json.dumps(rows,indent=2),encoding="utf8")
    if any(r["median_absolute_cents_from_target"]>25 or r["within_50_cents_fraction"]<.85 for r in rows):
        raise SystemExit("FAIL recorded-vowel output identity; inspect metrics.json")


if __name__=="__main__":main()
