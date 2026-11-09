"""Original cosmetic paper/leaf meshes; centimetres, no collision."""
from pathlib import Path
import math,json
P=Path(__file__).resolve().parent;P.mkdir(parents=True,exist_ok=True)
class Mesh:
 def __init__(self):self.v=[];self.f=[]
 def prism(self,poly,depth=1.2,mat='Paper'):
  # Polygon in local YZ; local X is the shallow paper thickness.
  n=len(poly);b=len(self.v);self.v +=[(-depth,y,z)for y,z in poly]+[(depth,y,z)for y,z in poly]
  self.f +=[(mat,tuple(b+i for i in reversed(range(n)))),(mat,tuple(b+n+i for i in range(n)))]
  for i in range(n):j=(i+1)%n;self.f.append(('Trim',(b+i,b+j,b+n+j,b+n+i)))
 def band(self,a,b,width=2.2):
  dy=b[0]-a[0];dz=b[1]-a[1];s=width/(2*math.hypot(dy,dz));ny=-dz*s;nz=dy*s
  self.prism([(a[0]+ny,a[1]+nz),(b[0]+ny,b[1]+nz),(b[0]-ny,b[1]-nz),(a[0]-ny,a[1]-nz)],.9,'Trim')
 def ring(self,points):
  for i,a in enumerate(points):self.band(a,points[(i+1)%len(points)])
 def save(self,name):
  lines=['# Original cosmetic role accessory. centimetres. No collision.']+['v %.5f %.5f %.5f'%v for v in self.v]
  lines+=['vt %.5f %.5f'%((y+110)/220,(z+50)/100)for x,y,z in self.v]
  # Explicit face UVs keep importer/editor behaviour stable without external textures.
  for mat,face in self.f:
   lines.append('usemtl '+mat);lines.append('f '+' '.join(str(i+1)+'/'+str(i+1)for i in face))
  (P/(name+'.obj')).write_text('\n'.join(lines)+'\n',encoding='utf-8')
  return {'name':name,'vertices':len(self.v),'triangles':sum(len(f)-2 for _,f in self.f)}
r=[]
m=Mesh()
for cy,cz,sy,sz in [(-16,3,13,21),(16,3,13,21),(0,16,11,20)]:
 m.ring([(cy,cz+sz),(cy+sy,cz),(cy,cz-sz),(cy-sy,cz)])
m.prism([(-4,-23),(4,-23),(4,-36),(-4,-36)],1,'Paper');r.append(m.save('SM_HealerLotusSeal'))
m=Mesh();m.ring([(0,34),(12,14),(0,-5),(-12,14)])
m.band((0,30),(0,-30),3)
for side in [-1,1]:m.prism([(side*5,0),(side*17,-8),(side*13,-31),(side*6,-22)],1.1)
r.append(m.save('SM_HealerNeedleSeal'))
m=Mesh()
for side in [-1,1]:
 m.prism([(side*12,7),(side*34,37),(side*89,25),(side*78,2),(side*51,-3),(side*69,-21),(side*48,-30),(side*18,-9)],1.4)
 m.band((side*16,5),(side*75,23),3)
 m.prism([(side*54,15),(side*63,24),(side*72,16),(side*63,7)],1.8,'Trim')
r.append(m.save('SM_MoonMothPaperWings'))
m=Mesh()
for side in [-1,1]:
 for step in range(3):
  y=18+step*15;z=15-step*13
  m.prism([(side*y,z),(side*(103-step*18),z+24),(side*(69-step*9),z-3),(side*(y+4),z-8)],1.3)
 m.band((side*14,10),(side*72,-9),3)
r.append(m.save('SM_WindReaverPaperFans'))
m=Mesh();m.ring([(0,43),(23,28),(29,0),(17,-23),(-17,-23),(-29,0),(-23,28)])
m.prism([(-17,25),(17,25),(21,0),(12,-17),(-12,-17),(-21,0)],2.6)
for y in [-12,0,12]:m.band((y,26),(y,-17),1.8)
m.prism([(-3,-25),(3,-25),(5,-46),(0,-40),(-5,-46)],1.1,'Trim')
r.append(m.save('SM_StarLanternPaperCage'))
(P/'Meshes.json').write_text(json.dumps(r,indent=2),encoding='utf-8');print(json.dumps(r))
