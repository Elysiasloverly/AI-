from pathlib import Path
import math,json
O=Path(__file__).resolve().parent;O.mkdir(exist_ok=True)
class Mesh:
 def __init__(self):self.v=[];self.f=[]
 def face(self,points,mat):
  b=len(self.v);self.v+=points;self.f.append((mat,tuple(range(b,b+len(points)))))
 def lathe(self,rings,sides=8,center=(0,0,0),mat='Body',angle=0):
  b=len(self.v);cx,cy,cz=center
  for z,r in rings:
   self.v +=[(cx+r*math.cos(angle+i*math.tau/sides),cy+r*math.sin(angle+i*math.tau/sides),cz+z)for i in range(sides)]
  self.f +=[(mat,tuple(b+i for i in reversed(range(sides)))),(mat,tuple(b+(len(rings)-1)*sides+i for i in range(sides)))]
  for j in range(len(rings)-1):
   for i in range(sides):n=(i+1)%sides;self.f.append((mat,(b+j*sides+i,b+j*sides+n,b+(j+1)*sides+n,b+(j+1)*sides+i)))
 def box(self,c,size,mat='Body'):
  x,y,z=c;a,b,d=[n/2 for n in size]
  p=[(x+i*a,y+j*b,z+k*d)for k in [-1,1]for j in [-1,1]for i in [-1,1]]
  for f in [(0,2,3,1),(4,5,7,6),(0,1,5,4),(2,6,7,3),(0,4,6,2),(1,3,7,5)]:self.face([p[i]for i in f],mat)
 def leaf(self,c,l,w,angle=0,mat='Trim'):
  x,y,z=c;d=(math.cos(angle),math.sin(angle));n=(-d[1],d[0]);self.face([(x,y,z),(x+d[0]*l*.45+n[0]*w,y+d[1]*l*.45+n[1]*w,z+7),(x+d[0]*l,y+d[1]*l,z+12),(x+d[0]*l*.45-n[0]*w,y+d[1]*l*.45-n[1]*w,z+6)],mat)
 def save(self,n):
  lines=['# Original themed set dressing. Centimeters. Cosmetic; collision disabled.']+['v %.5f %.5f %.5f'%p for p in self.v]
  lines+=['vt %.5f %.5f'%((x+200)/400,(z+20)/350)for x,y,z in self.v]
  for mat,f in self.f:lines+=['usemtl '+mat,'f '+' '.join(f'{i+1}/{i+1}'for i in f)]
  (O/(n+'.obj')).write_text('\n'.join(lines)+'\n',encoding='utf-8')
  return {'name':n,'vertices':len(self.v),'triangles':sum(len(f)-2 for _,f in self.f)}
R=[]
m=Mesh()
for x,y,h,rad in [(-26,0,92,34),(25,18,67,27),(0,-33,46,21)]:
 m.lathe([(0,rad*.3),(h*.7,rad*.22)],8,(x,y,0),'Body');m.lathe([(h*.68,rad*.5),(h*.76,rad),(h*.90,rad*.85),(h,rad*.2)],10,(x,y,0),'Trim')
 for a in [0,2,4]:m.leaf((x,y,2),40,8,a,'Light')
m.lathe([(0,35),(10,40),(23,25)],7,(45,-24,0),'Body');R.append(m.save('SM_ForestSpiritGarden'))
m=Mesh();m.lathe([(0,43),(10,43),(17,33),(25,33)],8,mat='Body');m.lathe([(25,19),(91,14)],8,mat='Body');m.lathe([(90,37),(99,42),(108,33)],4,mat='Trim',angle=math.pi/4)
m.box((0,0,132),(45,45,49),'Light')
for x in [-25,25]:
 for y in [-25,25]:m.box((x,y,133),(5,5,60),'Trim')
for z in [113,134,154]:m.box((0,0,z),(56,56,3),'Trim')
m.lathe([(160,41),(166,52),(180,24),(188,4)],4,mat='Trim',angle=math.pi/4);m.lathe([(188,4),(196,2)],8,mat='Light');R.append(m.save('SM_ShrineVotiveLantern'))
m=Mesh()
for x,y,h,r in [(-27,0,140,19),(19,18,95,25),(10,-31,68,16)]:m.lathe([(0,r*.65),(h*.13,r),(h*.78,r*.8),(h,r*.05)],6,(x,y,0),'Light')
for x,y in [(-55,23),(50,10),(34,-44)]:
 m.lathe([(0,9),(65,3)],5,(x,y,0),'Body');m.leaf((x,y,38),45,5,math.atan2(y,x),'Trim')
R.append(m.save('SM_IceShoreCrystalReeds'))
m=Mesh()
for i,(x,y,h)in enumerate([(-28,0,235),(20,20,270),(10,-25,180)]):
 m.lathe([(0,7),(h,5)],7,(x,y,0),'Body')
 for z in range(25,h,42):m.lathe([(z-2,8),(z+2,8)],7,(x,y,0),'Trim')
 for j in range(4):m.leaf((x,y,h-25-j*30),65-j*4,9,i+j*1.3,'Trim')
 m.box((x+12,y,h*.65),(12,2,38),'Light')
R.append(m.save('SM_BambooPrayerCluster'))
m=Mesh();m.lathe([(0,51),(13,54),(25,43)],8,mat='Body')
for i in range(24):
 if i in [3,4,5,6]:continue
 a=i*math.tau/24;b=(i+1)*math.tau/24
 def v(r,ang,x):return(x,r*math.cos(ang),105+r*math.sin(ang))
 m.face([v(59,a,-5),v(71,a,-5),v(71,b,-5),v(59,b,-5)],'Trim');m.face([v(59,a,5),v(59,b,5),v(71,b,5),v(71,a,5)],'Trim')
 m.face([v(71,a,-5),v(71,a,5),v(71,b,5),v(71,b,-5)],'Body')
 for a in [i*math.tau/24]:m.face([v(52,a-.02,-6),v(63,a-.02,-6),v(63,a+.02,-6),v(52,a+.02,-6)],'Light')
m.face([(-8,-3,103),(-8,30,151),(-8,3,108)],'Light');m.face([(-8,-3,103),(-8,-32,84),(-8,3,108)],'Light')
for a in [0,2.5,4.5]:m.leaf((0,0,12),80,10,a,'Trim')
R.append(m.save('SM_ClockGardenRelic'))
(O/'Meshes.json').write_text(json.dumps(R,indent=2),encoding='utf-8');print(R)
