from pathlib import Path
import numpy as np,wave,json
W=Path(__file__).parent;sr=48000;stats=[]
for k in range(3):
 rng=np.random.default_rng(4104+k);t=np.arange(int(sr*.32))/sr;n=len(t)
 white=rng.normal(0,1,n);spec=np.fft.rfft(white);f=np.fft.rfftfreq(n,1/sr)
 band=(1-np.exp(-(f/280)**4))*np.exp(-(f/(1900+k*90))**4);air=np.fft.irfft(spec*band,n);air/=max(.001,air.std())
 env=(1-np.exp(-t/.018))*np.exp(-((t-.09)/.105)**2)*(1-np.exp(-(t.max()-t)/.018))
 sweep=np.sin(2*np.pi*((610+k*25)*t-380*t*t))*np.exp(-t/0.10)*.035
 chime=sum(np.sin(2*np.pi*hz*t+seed)*a for hz,seed,a in[(660+k*20,.1,.035),(990+k*30,.7,.012),(1320+k*40,1.2,.004)])
 chime*=np.exp(-t/.12)*(1-np.exp(-t/.035))
 s=air*env*.095+sweep+chime;s-=s.mean();s=np.tanh(s*1.2);s*=.34/max(.001,abs(s).max())
 path=W/f'S_FrostMoonSlash_0{k+1}.wav'
 with wave.open(str(path),'wb')as out:out.setnchannels(1);out.setsampwidth(2);out.setframerate(sr);out.writeframes((s*32767).astype('<i2').tobytes())
 stats.append({'file':path.name,'duration':n/sr,'peak_dbfs':float(20*np.log10(abs(s).max())),'rms_dbfs':float(20*np.log10(np.sqrt(np.mean(s*s))))})
(W/'sound-report.json').write_text(json.dumps(stats,indent=2),encoding='utf-8');print(json.dumps(stats))
