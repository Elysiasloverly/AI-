"""Normal-battle capture with isolated profiles and an asset-readiness barrier.

This launches and exits disposable game processes. The original local progression
is copied, never overwritten. Output directories must be new for each invocation.
"""
from pathlib import Path
import subprocess,hashlib,json,shutil,time,argparse

project=Path(__file__).resolve().parents[1]
parser=argparse.ArgumentParser()
parser.add_argument('--scenes',nargs='+',choices=['day1','day20','night36','night39','coop39'],default=['day20','night39','coop39'])
parser.add_argument('--repeat',type=int,default=3)
parser.add_argument('--seconds',type=int,default=60)
parser.add_argument('--folder',default=time.strftime('%Y%m%d_%H%M%S'))
parser.add_argument('--output',type=Path)
parser.add_argument('--editor',default=r'C:\Program Files\Epic Games\UE_5.5\Engine\Binaries\Win64\UnrealEditor.exe')
parser.add_argument('--warmup',type=int,default=8)
parser.add_argument('--orbit',action='store_true')
parser.add_argument('--trace',action='store_true')
parser.add_argument('--level',type=int)
parser.add_argument('--defensive',action='store_true')
args=parser.parse_args()
import re
if not re.fullmatch(r'[A-Za-z0-9_-]{1,64}',args.folder):parser.error('--folder must use 1-64 letters, digits, underscores or hyphens')
if not 1<=args.repeat<=10 or not 5<=args.seconds<=600 or args.warmup<2:parser.error('repeat: 1-10; seconds: 5-600; warmup: at least 2')
if args.level is not None and not 1<=args.level<=80:parser.error('level must be between 1 and 80')
out=args.output.resolve()if args.output else project/'Saved/Profiling/RealBattle'/args.folder
out.mkdir(parents=True,exist_ok=False)
saves=project/'Saved/SaveGames'
originals={p.name:hashlib.sha256(p.read_bytes()).hexdigest()for p in [saves/'Baka_UpgradeTree_Player0.sav',saves/'Baka_UpgradeTree_Player0_backup.sav']}
(out/'original-saves.json').write_text(json.dumps(originals,indent=2),encoding='utf-8')
cases={'day1':('Battle_Day',1,1,1),'day20':('Battle_Day',20,21,1),'night36':('Battle_Night',36,41,1),'night39':('Battle_Night',39,41,1),'coop39':('Battle_Night',39,41,2)}
for scene in args.scenes:
 mapname,wave,level,players=cases[scene]
 if args.level is not None:level=args.level
 for rep in range(1,args.repeat+1):
  barrier=f'{args.folder}_{scene}_{rep}';processes=[];logs=[]
  try:
   for role in (['solo']if players==1 else['host','client']):
    label=f'{scene}_{role}_{rep}';slot=f'PerfP0_{args.folder}_{label}'
    assert not (out/f'{label}.log').exists(),f'Refusing overwrite {label}'
    for suffix in ['', '_backup']:
     src=saves/('Baka_UpgradeTree_Player0'+suffix+'.sav');dst=saves/(slot+suffix+'.sav')
     assert dst.resolve().parent==saves.resolve()and dst.name.startswith('PerfP0_')and dst.resolve()!=src.resolve()
     shutil.copy2(src,dst)
    url=(f'/Game/Map/{mapname}'+('?listen'if role=='host'else''))if role!='client'else'127.0.0.1:7792'
    cmd=[args.editor,str(project/'Baka.uproject'),url,'-game','-RenderOffscreen','-windowed','-ResX=1920','-ResY=1080','-ForceRes','-NoVSync','-unattended','-nop4','-SCCProvider=None','-NoSplash','-UTF8Output',f'-RogueProgressionSlot={slot}',f'-abslog={out/label}.log',f'-ExecCmds=t.MaxFPS 0,r.VSync 0,r.GPUCsvStatsEnabled 1,Rogue.Perf.RealBattle {label} {wave} {level} {players} {args.warmup} {args.seconds} {out.as_posix()}']
    if not args.orbit:cmd.append('-RoguePerfAdaptiveSteering')
    if args.defensive:cmd.append('-RoguePerfDefensiveBuild')
    if role=='host':cmd.append('-port=7792')
    if players==2:cmd.append(f'-RoguePerfBarrier={barrier}')
    if args.trace and role!='client':cmd+=['-trace=cpu,frame,bookmark,loadtime','-statnamedevents',f'-tracefile={out/label}.utrace']
    (out/f'{label}.command.json').write_text(json.dumps(cmd,indent=2),encoding='utf-8')
    f=(out/f'{label}.console.log').open('w');logs.append(f)
    print('START',args.folder,label,flush=True)
    p=subprocess.Popen(cmd,stdout=f,stderr=subprocess.STDOUT,creationflags=subprocess.CREATE_NO_WINDOW);processes.append((label,p))
    if role=='host':time.sleep(3)
   for label,p in processes:
    try:code=p.wait(timeout=args.seconds+args.warmup+200)
    except subprocess.TimeoutExpired:p.kill();p.wait();code=-999
    print('END',label,code,flush=True)
    assert code==0 and (out/f'{label}.csv').exists()and(out/f'{label}.result.txt').exists(),f'Invalid capture {label}'
  finally:
   for _,p in processes:
    if p.poll()is None:p.kill();p.wait()
   for f in logs:f.close()
   for name,digest in originals.items():assert hashlib.sha256((saves/name).read_bytes()).hexdigest()==digest,'Original profile changed!'
print('Completed; original saves unchanged.',flush=True)
