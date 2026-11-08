from pathlib import Path
import wave
import numpy as np
assets=Path(__file__).resolve().parent
# Original ambient composition: soft suspended chords and sparse bell notes.
rate=44100;duration=32;t=np.arange(rate*duration)/rate;stereo=np.zeros((len(t),2))
chords=[[146.83,220,293.66,369.99],[130.81,196,261.63,329.63],[164.81,246.94,329.63,440],[146.83,220,293.66,392]]
for j,chord in enumerate(chords):
    phase=(t-j*8)%duration;env=np.where(phase<12,np.sin(np.pi*np.minimum(phase,12)/12)**2,0)
    for k,f in enumerate(chord):
        f=round(f*duration)/duration
        stereo[:,0]+=env*(np.sin(2*np.pi*f*t)+.14*np.sin(2*np.pi*f*2*t))*.033
        stereo[:,1]+=env*(np.sin(2*np.pi*f*t+.10*k)+.14*np.sin(2*np.pi*f*2*t))*.033
for j,f in enumerate([587.33,739.99,659.25,880,739.99,659.25,587.33,783.99]):
    age=(t-j*4)%duration;env=(1-np.exp(-age*8))*np.exp(-age*1.2);f=round(f*duration)/duration
    tone=(np.sin(2*np.pi*f*t)+.2*np.sin(2*np.pi*2*f*t))*env*.025
    stereo[:,0]+=tone*(.8 if j%2 else 1);stereo[:,1]+=tone*(1 if j%2 else .8)
pcm=(np.clip(stereo,-.5,.5)*32767).astype('<i2')
with wave.open(str(assets/'music.wav'),'wb') as w:w.setnchannels(2);w.setsampwidth(2);w.setframerate(rate);w.writeframes(pcm.tobytes())
