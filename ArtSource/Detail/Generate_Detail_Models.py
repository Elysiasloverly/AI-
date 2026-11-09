from pathlib import Path
import math,json
P=Path(__file__).parent;P.mkdir(parents=True,exist_ok=True)
def save(name,verts,faces):
    lines=['# Original low-poly cosmetic mesh, centimetres. No collision geometry.']
    lines+=['v %.6f %.6f %.6f'%tuple(v)for v in verts]
    lines+=['f '+' '.join(str(i+1)for i in f)for f in faces]
    (P/(name+'.obj')).write_text('\n'.join(lines)+'\n',encoding='utf-8')
    return {'name':name,'vertices':len(verts),'triangles':sum(len(f)-2 for f in faces)}
r=[]
# Six-sided spindle, with broad flat facets and a smaller crown. Same authored 100cm height as the former sphere.
v=[(0,0,50)]+[(math.cos(i*math.tau/6)*30,math.sin(i*math.tau/6)*30,8)for i in range(6)]+[(0,0,-50)]
f=[]
for i in range(6):j=(i+1)%6;f.extend([(0,1+i,1+j),(7,1+j,1+i)])
r.append(save('SM_SpiritCrystal',v,f))
v=[];f=[]
def prism(poly,depth=2):
    n=len(poly);base=len(v);v.extend([(x,y,-depth)for x,y in poly]);v.extend([(x,y,depth)for x,y in poly])
    f.append(tuple(base+i for i in reversed(range(n))));f.append(tuple(base+n+i for i in range(n)))
    for i in range(n):j=(i+1)%n;f.append((base+i,base+j,base+n+j,base+n+i))
points=[(0,42),(34,0),(0,-42),(-34,0)]
for i,a in enumerate(points):
    b=points[(i+1)%4];dx=b[0]-a[0];dy=b[1]-a[1];l=math.hypot(dx,dy);nx=-dy/l*2.8;ny=dx/l*2.8
    prism([(a[0]+nx,a[1]+ny),(b[0]+nx,b[1]+ny),(b[0]-nx,b[1]-ny),(a[0]-nx,a[1]-ny)])
# Three folded paper tails below the hollow crest, leaving the centre open.
for x,y in [(-14,-34),(0,-44),(14,-34)]:
    prism([(x-4,y),(x+4,y),(x+4,y-9),(x-1,y-9),(x-1,y-18),(x-8,y-18),(x-8,y-12),(x-4,y-12)],1)
r.append(save('SM_BossSealCrest',v,f))
(P/'Meshes.json').write_text(json.dumps(r,indent=2),encoding='utf-8');print(json.dumps(r))
