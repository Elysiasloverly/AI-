"""Frost Moon V2: original ice katana with faceted star guard and restrained graphic FX."""
from pathlib import Path
from math import sin,cos,pi,sqrt
import json
W=Path(__file__).parent
class Obj:
 def __init__(self):self.v=[];self.faces=[]
 def face(self,vs,mat=0,uv=None):
  ids=[]
  for i,p in enumerate(vs):self.v.append((p,uv[i]if uv else(p[0]/190,.5+p[1]/40)));ids.append(len(self.v))
  for i in range(1,len(ids)-1):self.faces.append((mat,[ids[0],ids[i],ids[i+1]]))
 def save(self,name):
  s=['# FrostMoon V2, centimetres, blade points along local +X.','mtllib FrostMoonV2.mtl','o '+name]
  s+=['v '+' '.join(f'{n:.6f}'for n in p)for p,uv in self.v];s+=['vt '+' '.join(f'{n:.6f}'for n in uv)for p,uv in self.v]
  last=None
  for m,ids in self.faces:
   if m!=last:s.append('usemtl Slot'+str(m));last=m
   s.append('f '+' '.join(f'{i}/{i}'for i in ids))
  (W/(name+'.obj')).write_text('\n'.join(s)+'\n',encoding='utf-8')
 def rod(self,a,b,r0,r1,mat,n=12):
  for i in range(n):
   t,u=2*pi*i/n,2*pi*(i+1)/n
   self.face([(a,r0*cos(t),r0*sin(t)),(b,r1*cos(t),r1*sin(t)),(b,r1*cos(u),r1*sin(u)),(a,r0*cos(u),r0*sin(u))],mat)
  self.face([(a,r0*cos(-i*2*pi/n),r0*sin(-i*2*pi/n))for i in range(n)],mat);self.face([(b,r1*cos(i*2*pi/n),r1*sin(i*2*pi/n))for i in range(n)],mat)
 def crystal(self,x,y,z,l,w,h,mat=4,seed=0):
  top=(x,y,z+h);bottom=(x,y,z-h);belt=[(x-l,y,z),(x,y-w,z),(x+l,y,z),(x,y+w,z)]
  for i in range(4):
   self.face([top,belt[i],belt[(i+1)%4]],mat,[(seed,.5),(seed+.45,0),(seed+.9,1)]);self.face([bottom,belt[(i+1)%4],belt[i]],mat,[(seed,.5),(seed+.9,1),(seed+.45,0)])
blade=Obj();xs=[41,48,66,88,112,138,161,176,185,190];ws=[12,17,17,16,15,13,10,6,3,.05]
def cross(x,w):
 c=6*((x-41)/149)**2;t=3.2*(w/17)**.35
 return[(x,c-w,0),(x,c-w*.82,t*.4),(x,c-w*.4,t),(x,c+w*.45,t*.65),(x,c+w,0),(x,c+w*.45,-t*.65),(x,c-w*.4,-t),(x,c-w*.82,-t*.4)]
for i in range(len(xs)-1):
 a,b=cross(xs[i],ws[i]),cross(xs[i+1],ws[i+1])
 for j in range(8):blade.face([a[j],b[j],b[(j+1)%8],a[(j+1)%8]], [4,0,0,1,1,0,0,4][j])
# Asymmetric frost veins: long negative spaces rather than a repeated checker pattern.
for side in[-1,1]:
 for i in range(5):
  x=57+i*23;c=6*((x-41)/149)**2;z=side*3.35;w=7-i*.8
  blade.face([(x,c-w,z),(x+9,c+1,z),(x+18,c-w*.5,z),(x+9,c-w*.25,z)],4)
  blade.face([(x+2,c+w*.5,z),(x+8,c-w*.2,z),(x+10,c+w*.4,z)],1)
 # Fine dark spine line makes the white edge readable on light stone.
 blade.face([(45,10,side*2),(160,10,side*2),(177,7,side*1.9),(62,12,side*2.2)],5)
# Six-point star guard with a real crystalline ridge and dark silver sockets.
for i in range(6):
 a=i*pi/3;v=(cos(a),sin(a));n=(-v[1],v[0]);r=25 if i%3 else 29
 y,z=v[0]*r,v[1]*r;by,bz=v[0]*6,v[1]*6
 left=(by+n[0]*5,bz+n[1]*5);right=(by-n[0]*5,bz-n[1]*5)
 for xx,sign in[(36,-1),(43,1)]:
  ridge=(xx+sign*2,v[0]*15,v[1]*15)
  blade.face([(xx,*left),(xx,y,z),ridge],0);blade.face([(xx,y,z),(xx,*right),ridge],4);blade.face([(xx,*right),(xx,*left),ridge],1)
 blade.face([(36,*left),(43,*left),(43,y,z),(36,y,z)],2);blade.face([(36,y,z),(43,y,z),(43,*right),(36,*right)],2)
blade.rod(33,45,7,7,2);blade.rod(1,32,4.8,5.0,1)
for x in[2,7,12,17,22,27]:
 for i in range(4):
  a=i*pi/2;y,z=cos(a)*5.1,sin(a)*5.1;ny,nz=-sin(a)*3.8,cos(a)*3.8
  blade.face([(x,y,z),(x+2.5,y+ny,z+nz),(x+5,y,z),(x+2.5,y-ny,z-nz)],3)
blade.rod(-2,1,6,4.8,2);blade.rod(31,34,5.5,5.5,2);blade.crystal(-5,0,0,6,5.8,5.8,0)
blade.crystal(40,0,0,5,8,8,4);blade.save('SM_FrostMoonBlade_V2')
motes=Obj()
for i in range(9):
 x=45+i*16;y=(-1 if i%2 else 1)*(18+5*sin(i*1.9));z=5+8*sin(i*.9)
 motes.crystal(x,y,z,2.5+(i%3),1.1,1.8,0,i*2)
motes.save('SM_FrostMoonMotes')
wave=Obj()
for layer in range(2):
 for i in range(64):
  t0=-1+2*i/64;t1=-1+2*(i+1)/64
  def p(t,v):
   scale=1 if layer==0 else .93;y=t*240*scale;front=50*(1-t*t)*scale-(0 if layer==0 else 19);thick=(44 if layer==0 else 12)*(1-abs(t)**1.35)
   return(front-thick*v,y,0 if layer==0 else -1)
  wave.face([p(t0,0),p(t1,0),p(t1,1),p(t0,1)],0,[(layer*2,(t0+1)/2),(layer*2,(t1+1)/2),(layer*2+1,(t1+1)/2),(layer*2+1,(t0+1)/2)])
for sign in[-1,1]:
 for i in range(4):
  y=sign*(150+i*23);x=-15+i*6;w=1.2+i*.15;l=5+i
  wave.face([(x-l,y,0),(x,y-w,0),(x+l,y,0),(x,y+w,0)],0,[(4,.5),(4.5,0),(5,.5),(4.5,1)])
wave.save('SM_FrostMoonWave_V2')
trail=Obj()
for i in range(48):
 a,b=-84*pi/180*i/48,-84*pi/180*(i+1)/48
 def p(t,r):return(r*cos(t),r*sin(t),0)
 trail.face([p(a,60),p(a,190),p(b,190),p(b,60)],0,[(0,i/48),(1,i/48),(1,(i+1)/48),(0,(i+1)/48)])
trail.save('SM_FrostMoonSlash_V2')
(W/'FrostMoonV2.mtl').write_text('\n'.join(f'newmtl Slot{i}\nKd .15 .55 .8\n'for i in range(6)),encoding='utf-8')
(W/'models.json').write_text(json.dumps({n:len(o.faces)for n,o in [('blade',blade),('motes',motes),('wave',wave),('trail',trail)]},indent=2),encoding='utf-8')
print((W/'models.json').read_text(encoding='utf-8'))
