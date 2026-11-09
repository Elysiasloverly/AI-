from pathlib import Path
import math,wave,struct,json,shutil,random
W=Path(__file__).parent
OUT=W;OUT.mkdir(exist_ok=True)
summary=[]
for name,boss in [('SFX_SpiritDefeat',False),('SFX_BossSealResolve',True)]:
    sr=48000;length=.38 if boss else .14;peak=.32 if boss else .24
    data=[];noise=random.Random(73 if boss else 31);lp_noise=0;lp1=lp2=0;phase=0
    a=1-math.exp(-2*math.pi*1100/sr)
    for i in range(int(sr*length)):
        t=i/sr
        # Rounded wooden/air tick: descending low-mid tone, restrained harmonics, no metallic ringing.
        hz=(330+100*math.exp(-t*32)) if boss else (285+130*math.exp(-t*45))
        phase+=2*math.pi*hz/sr
        env=(1-math.exp(-t*220))**2*math.exp(-t*(18 if boss else 32))
        v=env*(math.sin(phase)+.055*math.sin(phase*2)*math.exp(-t*35))
        lp_noise+=.06*(noise.uniform(-1,1)-lp_noise)
        v+=.06*lp_noise*(1-math.exp(-t*180))*math.exp(-t*60)
        if boss:
            u=t-.085
            if u>=0:v+=.30*(1-math.exp(-u*150))**2*math.exp(-u*18)*math.sin(2*math.pi*261.63*u)
        edge=max(0,min(1,(length-t)/.022));v*=edge*edge*(3-2*edge)
        lp1+=a*(v-lp1);lp2+=a*(lp1-lp2);data.append(lp2)
    gain=peak/max(map(abs,data));data=[v*gain for v in data]
    with wave.open(str(OUT/(name+'.wav')),'wb')as f:
        f.setnchannels(1);f.setsampwidth(2);f.setframerate(sr);f.writeframes(b''.join(struct.pack('<h',round(v*32767))for v in data))
    summary.append({'name':name,'seconds':length,'peak_dbfs':20*math.log10(max(map(abs,data))),'rms_dbfs':20*math.log10(math.sqrt(sum(v*v for v in data)/len(data)))})
(W/'generated.json').write_text(json.dumps(summary,indent=2),encoding='utf-8')
print(json.dumps(summary))
