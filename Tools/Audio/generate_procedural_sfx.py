"""Baka original SFX, revision 3. Python + NumPy; no external samples.

Asset paths remain stable for reimport. Silver Blade intentionally has no audio.
Music, playback cadence and per-player volume settings are not generated here.
"""
from pathlib import Path
import argparse, hashlib, json, math, wave
import numpy as np

RATE = 48000
REVISION = 3
DURATIONS = {'Projectile': .23, 'Laser': .30, 'HellTower': .19, 'Rocket': .34,
             'Mortar': .43, 'Enemy': .10, 'Dash': .19, 'Experience': .12,
             'Coin': .17, 'ShieldBreak': .40, 'UIHover': .068, 'UIConfirm': .14,
             'LevelUp': .73, 'Revive': .88}
# Mastering uses the audible front portion rather than equalizing every peak.
LEVELS = {'Projectile': (-20.5,.12,.46,7800), 'Laser': (-22.0,.15,.46,6400),
          'HellTower': (-25.2,.13,.36,5100), 'Rocket': (-20.2,.15,.49,6600),
          'Mortar': (-20.6,.17,.51,7000), 'Enemy': (-26.0,.060,.25,4500),
          'Dash': (-24.0,.12,.40,6500), 'Experience': (-26.0,.09,.29,5900),
          'Coin': (-24.8,.11,.31,6600), 'ShieldBreak': (-23.0,.20,.48,7300),
          'UIHover': (-27.5,.045,.20,4600), 'UIConfirm': (-24.7,.080,.30,5000),
          'LevelUp': (-25.0,.60,.43,6600), 'Revive': (-25.0,.70,.43,6600)}

def clock(seconds): return np.arange(round(seconds*RATE),dtype=np.float64)/RATE
def attack(t,tau=.003): return -np.expm1(-np.maximum(t,0)/tau)
def decay(t,tau=.05): return np.exp(-np.maximum(t,0)/tau)
def glide(t,a,b,tau): return b+(a-b)*decay(t,tau)
def osc(t,f,phase=0): return np.sin(2*np.pi*np.cumsum(np.broadcast_to(f,t.shape))/RATE+phase)

def filter_signal(x,low=35,high=8000,deharsh=0):
    # Zero padding avoids wrapping launch transients into the quiet end of a clip.
    pad=round(.04*RATE);size=len(x)+pad*2
    f=np.fft.rfftfreq(size,1/RATE)
    hp=(f/max(1,low))**4/(1+(f/max(1,low))**4)
    lp=np.exp(-.5*(f/high)**6)
    notch=1-deharsh*np.exp(-.5*((f-3400)/1100)**2)
    return np.fft.irfft(np.fft.rfft(np.pad(x,(pad,pad)))*hp*lp*notch,n=size)[pad:pad+len(x)]

def noise(t,rng,low=100,high=6500):
    x=filter_signal(rng.normal(size=len(t)),low,high)
    return x/(np.sqrt(np.mean(x*x))+1e-12)*.3

def resonance(t,rng,f,tau,ratios=(1,1.46,2.19),brightness=.55):
    x=np.zeros_like(t)
    for i,ratio in enumerate(ratios):
        freq=f*ratio*rng.uniform(.994,1.006)
        x+=(brightness**i)*osc(t,glide(t,freq*1.008,freq,.010),rng.uniform(0,6.28))*decay(t,tau/(1+i*.45))
    return x*attack(t,.0015)

def delay(x,seconds,gain):
    y=x.copy();n=round(seconds*RATE)
    if n<len(x): y[n:]+=gain*x[:-n]
    return y

def master(x,role):
    target,window,peak,cutoff=LEVELS[role]
    x=filter_signal(x,low=42 if role not in ('Mortar','Rocket') else 32,
                    high=cutoff,deharsh=.22 if role in ('Projectile','Laser','ShieldBreak') else .10)
    a=min(round(.0015*RATE),len(x)//5);r=min(round(.022*RATE),len(x)//3)
    x[:a]*=np.sin(np.linspace(0,np.pi/2,a))**2
    x[-r:]*=np.cos(np.linspace(0,np.pi/2,r))**2
    # Gentle saturation rounds isolated noise spikes; it does not hard clip PCM.
    head=x[:min(len(x),round(window*RATE))]
    x*=10**(target/20)/(np.sqrt(np.mean(head*head))+1e-12)
    x=peak*np.tanh(x/peak)
    head=x[:min(len(x),round(window*RATE))]
    gain=10**(target/20)/(np.sqrt(np.mean(head*head))+1e-12)
    x*=min(gain,peak/(np.max(np.abs(x))+1e-12))
    x[0]=x[-1]=0
    return x

def make(role,variant=0):
    rng=np.random.default_rng(20261027+variant*503+sum(map(ord,role))*43)
    t=clock(DURATIONS[role]);p=[.985,1.,1.018][variant%3]
    n=lambda lo,hi:noise(t,rng,lo,hi)
    if role=='Projectile':
        # Icy snap, directional cold-air body, then small separated crystal fragments.
        snap=n(850,6800)*attack(t,.0015)*decay(t,.020)
        air=n(1600,7000)*np.exp(-((t-(.029+variant*.0015))/.019)**2)
        x=.92*snap+.46*air
        x+=.16*resonance(t,rng,1780*p,.034,(1,1.37,2.14,2.66),.48)
        x+=.14*osc(t,glide(t,240*p,105,.016))*attack(t,.002)*decay(t,.024)
        for start,amp,f in [(.026,.052,2370),(.057,.030,3150),(.083,.012,1910)]:
            u=np.maximum(t-start,0)
            x+=(t>=start)*amp*resonance(u,rng,f*p,.019,(1,1.71),.43)
        x=delay(x,.017,.045)
    elif role=='Laser':
        # Focused energy ribbon: smoother FM, two detuned voices, short diffusion.
        freq=glide(t,1230*p,350*p,.060)
        phase=2*np.pi*np.cumsum(freq)/RATE
        env=attack(t,.0035)*decay(t,.062)
        fm=.36*np.sin(2*np.pi*np.cumsum(freq*1.43)/RATE)*decay(t,.075)
        x=(.34*np.sin(phase+fm)+.13*np.sin(phase*1.006+.4))*env
        x+=.43*n(600,5100)*attack(t,.0016)*decay(t,.023)
        x+=.32*n(140,2050)*attack(t,.010)*decay(t,.051)
        x=delay(delay(x,.012,.10),.026,.04)
    elif role=='HellTower':
        # Quiet, warm, compact flame texture so .25-second repetition remains readable.
        flame=attack(t,.004)*decay(t,.040)
        turbulence=.78+.11*np.sin(2*np.pi*31*t)+.06*np.sin(2*np.pi*79*t+.4)
        x=1.30*n(125,2350)*flame*turbulence
        x+=.13*osc(t,glide(t,182*p,95,.029))*flame
        pulses=np.zeros_like(t)
        for point in [.008,.021,.044,.071]:pulses+=np.exp(-((t-point)/rng.uniform(.0015,.0027))**2)
        x+=.23*n(1450,5100)*pulses*decay(t,.047)
    elif role=='Rocket':
        # Flame ignition and outward push, deliberately less tonal than the laser.
        x=.66*n(160,2800)*attack(t,.0018)*decay(t,.016)
        x+=1.45*n(90,3300)*attack(t,.010)*decay(t,.069)
        x+=.25*osc(t,glide(t,213*p,64,.027))*attack(t,.003)*decay(t,.055)
        x+=.30*n(650,5000)*np.exp(-((t-.048)/.032)**2)
        x=delay(x,.033,.055)
    elif role=='Mortar':
        # Hollow tube thump with ice-shell overtones; leaves room for a later impact.
        x=.65*osc(t,glide(t,131*p,47,.022))*attack(t,.0018)*decay(t,.058)
        x+=.20*osc(t,glide(t,285*p,143,.028))*attack(t,.002)*decay(t,.041)
        x+=.93*n(95,1900)*attack(t,.002)*decay(t,.031)
        u=np.maximum(t-.011,0)
        x+=(t>=.011)*.11*resonance(u,rng,1180*p,.081,(1,1.42,2.12,2.97),.46)
        x+=.21*n(1800,6400)*attack(t,.012)*decay(t,.068)
        x=delay(x,.046,.075)
    elif role=='Enemy':
        # Low-priority soft danmaku puff, not a piercing electronic whistle.
        x=.79*n(380,3000)*attack(t,.0017)*decay(t,.013)
        x+=.16*osc(t,glide(t,482,242,.015))*attack(t,.002)*decay(t,.017)
        x+=.09*osc(t,glide(t,152,96,.016))*attack(t,.002)*decay(t,.019)
    elif role=='UIHover':
        # Clear tactile entering cue, dry and brief, with no bell tail stacking on motion.
        x=.75*n(370,3100)*attack(t,.0016)*decay(t,.0085)
        x+=.20*resonance(t,rng,655,.012,(1,2.39,3.74),.38)
    elif role=='UIConfirm':
        x=.70*n(200,3400)*attack(t,.0015)*decay(t,.010)
        x+=.32*resonance(t,rng,492,.022,(1,2.63,4.08),.41)
        u=np.maximum(t-.009,0)
        x+=(t>=.009)*.033*resonance(u,rng,1396.9,.041,(1,2.03),.32)
    elif role=='Dash':
        env=np.exp(-((t-.037)/.023)**2)*attack(t,.002)
        x=1.05*n(320,5300)*env+.18*n(1400,6100)*attack(t,.006)*decay(t,.040)
        x+=.09*osc(t,glide(t,270,94,.024))*env
    elif role in ('Experience','Coin'):
        f=1046.5 if role=='Experience' else 1567.98
        x=.25*resonance(t,rng,f,.030,(1,2.01,2.75),.32)
        x+=.07*n(1050,4100)*attack(t,.0018)*decay(t,.006)
        if role=='Coin':
            u=np.maximum(t-.023,0)
            x+=(t>=.023)*.05*resonance(u,rng,f*1.498,.028,(1,2.13),.30)
    elif role=='ShieldBreak':
        x=.12*osc(t,glide(t,190,62,.032))*attack(t,.003)*decay(t,.046)
        x+=.20*resonance(t,rng,1210,.071,(1,1.46,2.19,2.77),.43)
        for start in [.004,.012,.023,.039,.067,.099,.135]:
            u=np.maximum(t-start,0)
            x+=(t>=start)*rng.uniform(.2,.7)*n(1000,6900)*attack(u,.0008)*decay(u,.0045)
    else:
        # Sparse major-pentatonic chimes: UI feedback, with no sampled music content.
        notes=[523.25,587.33,783.99,1046.5] if role=='LevelUp' else [392.,523.25,587.33,783.99]
        x=np.zeros_like(t)
        for i,f in enumerate(notes):
            start=i*(.078 if role=='LevelUp' else .099);u=np.maximum(t-start,0)
            x+=(t>=start)*(.17 if i<3 else .13)*resonance(u,rng,f,.125 if role=='LevelUp' else .155,(1,2.006,3.012),.33)
        x+=.025*n(2000,5900)*attack(t,.030)*decay(t,.18)
        x=delay(x,.044,.08)
    return master(x,role)

def metrics(x,role=None):
    freq=np.fft.rfftfreq(len(x),1/RATE);power=np.abs(np.fft.rfft(x))**2
    peak=float(np.max(np.abs(x)));rms=float(np.sqrt(np.mean(x*x)))
    win=LEVELS[role][1] if role else min(.15,len(x)/RATE)
    active=float(np.sqrt(np.mean(x[:round(win*RATE)]**2)))
    db=lambda v:round(20*math.log10(max(v,1e-12)),2)
    energy=float(np.sum(x*x));tail_start=np.searchsorted(np.cumsum(x*x),energy*.99)
    return {'seconds':len(x)/RATE,'peak_dbfs':db(peak),'rms_dbfs':db(rms),'active_rms_dbfs':db(active),
        'centroid_hz':round(float(np.sum(freq*power)/max(np.sum(power),1e-12))),
        'energy_above_6khz_pct':round(float(np.sum(power[freq>=6000])/max(np.sum(power),1e-12)*100),3),
        'energy_99pct_seconds':round(float(tail_start/RATE),4),'dc_offset':round(float(np.mean(x)),8),
        'clipped_samples':int(np.count_nonzero(np.abs(x)>=.999)),'first_sample':float(x[0]),'last_sample':float(x[-1])}

def write_wave(path,x,role=None):
    path.parent.mkdir(parents=True,exist_ok=True)
    assert np.all(np.isfinite(x)) and np.max(np.abs(x))<.8,str(path)
    pcm=np.rint(x*32767).astype('<i2')
    with wave.open(str(path),'wb') as f:
        f.setnchannels(1);f.setsampwidth(2);f.setframerate(RATE);f.writeframes(pcm.tobytes())
    with wave.open(str(path),'rb') as f:
        assert f.getnchannels()==1 and f.getsampwidth()==2 and f.getframerate()==RATE
        decoded=np.frombuffer(f.readframes(f.getnframes()),'<i2').astype(float)/32768
    result=metrics(decoded,role)
    assert result['clipped_samples']==0 and abs(result['dc_offset'])<.002
    assert result['first_sample']==0 and result['last_sample']==0
    result['sha256']=hashlib.sha256(path.read_bytes()).hexdigest()
    return result

def reel(items,gap=.35):
    segments=[np.zeros(round(.15*RATE))];timeline=[];pos=.15
    for role,variant,label in items:
        clip=make(role,variant);timeline.append({'role':role,'variant':variant+1,'label':label,'start_seconds':round(pos,3)})
        segments.extend([clip,np.zeros(round(gap*RATE))]);pos+=len(clip)/RATE+gap
    return np.concatenate(segments),timeline

def main():
    parser=argparse.ArgumentParser();parser.add_argument('--output',type=Path,required=True)
    args=parser.parse_args();out=args.output;out.mkdir(parents=True,exist_ok=True)
    roles=[('Projectile','Ofuda_IceSpike','御札：冰锥发射'),('Laser','MasterSpark_Laser','极限火花：能量激光'),
           ('HellTower','Onbashira_FlameBeam','御柱：火焰激光'),('Rocket','Hakkero_Fireball','八卦炉：火球发射'),
           ('Mortar','YinYang_IceMortar','阴阳玉：冰球迫击炮')]
    assets=[]
    for role,name,label in roles:
        for variant in range(3):
            asset=f'SFX_{name}_{variant+1:02}'
            row=write_wave(out/'Weapons'/(asset+'.wav'),make(role,variant),role)
            assets.append({'role':role,'label':label,'asset':asset,'folder':'Weapons','variant':variant+1,**row})
    name='SFX_Enemy_Danmaku';row=write_wave(out/'Enemies'/(name+'.wav'),make('Enemy'), 'Enemy')
    assets.append({'role':'Enemy','label':'怪物：通用弹幕','asset':name,'folder':'Enemies','variant':1,**row})
    feedback={'Dash':'冲刺气流','Experience':'经验拾取','Coin':'金币拾取','ShieldBreak':'护盾破裂',
              'UIHover':'按钮悬停','UIConfirm':'按钮按下','LevelUp':'升级提示','Revive':'复活提示'}
    for role,label in feedback.items():
        asset='SFX_'+role;row=write_wave(out/'Feedback'/(asset+'.wav'),make(role),role)
        assets.append({'role':role,'label':label,'asset':asset,'folder':'Feedback','variant':1,**row})
    previews={}
    for filename,items,gap in [
        ('Weapons-Preview.wav',[(r,v,l) for r,n,l in roles for v in range(2)]+[('Enemy',0,'怪物弹幕')],.35),
        ('Buttons-Preview.wav',[(r,0,feedback[r]) for r in ['UIHover']*3+['UIConfirm']*3],.25),
        ('Feedback-Preview.wav',[(r,0,feedback[r]) for r in ['Dash','Experience','Coin','ShieldBreak','LevelUp','Revive']],.35)]:
        audio,timeline=reel(items,gap);write_wave(out/filename,audio);previews[filename]=timeline
    data={'revision':REVISION,'sample_rate':RATE,'channels':1,'pcm_bits':16,'original_procedural':True,
          'assets':assets,'previews':previews,'intentionally_removed_roles':['Scythe'],
          'mastering':'active-window RMS matching, softened transients, deharsh EQ, role-specific tail lengths'}
    (out/'manifest.json').write_text(json.dumps(data,ensure_ascii=False,indent=2),encoding='utf-8')
    print(f'Generated revision {REVISION}: {len(assets)} verified mono WAVs; Silver Blade excluded.')

if __name__=='__main__':main()
