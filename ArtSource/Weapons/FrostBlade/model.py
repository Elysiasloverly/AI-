"""Original faceted ice katana, +X blade axis / handle at origin. Centimetres."""
from pathlib import Path
from math import sin,cos,pi
import json
W=Path(__file__).parent
class Obj:
 def __init__(self):self.v=[];self.faces=[]
 def face(self,vs,mat=0,uv=None):
  ids=[]
  for i,p in enumerate(vs):self.v.append((p,(uv[i] if uv else (p[0]/190,.5+p[1]/36))));ids.append(len(self.v))
  for i in range(1,len(ids)-1):self.faces.append((mat,[ids[0],ids[i],ids[i+1]]))
 def save(self,name):
  lines=['# FrostMoon original mesh. +X forward, centimetres.','mtllib FrostMoon.mtl','o '+name]
  for p,uv in self.v:lines.append('v '+' '.join(f'{x:.5f}'for x in p))
  for p,uv in self.v:lines.append('vt '+' '.join(f'{x:.5f}'for x in uv))
  mat=None
  for m,ids in self.faces:
   if mat!=m:lines.append('usemtl Slot'+str(m));mat=m
   lines.append('f '+' '.join(f'{i}/{i}'for i in ids))
  (W/(name+'.obj')).write_text('\n'.join(lines)+'\n')
 def rod(self,x0,x1,r0,r1,mat=2,n=8,y=0,z=0):
  for i in range(n):
   a,b=i*2*pi/n,(i+1)*2*pi/n
   self.face([(x0,y+r0*cos(a),z+r0*sin(a)),(x1,y+r1*cos(a),z+r1*sin(a)),(x1,y+r1*cos(b),z+r1*sin(b)),(x0,y+r0*cos(b),z+r0*sin(b))],mat)
  self.face([(x0,y+r0*cos(-i*2*pi/n),z+r0*sin(-i*2*pi/n))for i in range(n)],mat)
  self.face([(x1,y+r1*cos(i*2*pi/n),z+r1*sin(i*2*pi/n))for i in range(n)],mat)

blade=Obj()
# Diamond bevel cross-section, shallow katana curvature, taper to needle tip.
xs=[42,52,78,108,140,165,180,190];width=[12,15,15,14,12,9,5,.08]
def section(x,w):
 c=5*((x-42)/148)**2;t=2.4*(w/15)**.4
 return [(x,c-w,0),(x,c-w*.68,t*.78),(x,c+w*.55,t),(x,c+w,0),(x,c+w*.55,-t),(x,c-w*.68,-t*.78)]
for i in range(len(xs)-1):
 a,b=section(xs[i],width[i]),section(xs[i+1],width[i+1])
 for j in range(6):blade.face([a[j],b[j],b[(j+1)%6],a[(j+1)%6]],0 if j in [0,1,4,5]else 1)
# Icy inlay facets on both broad faces.
for sign in [-1,1]:
 for i in range(6):
  x=54+i*18;c=5*((x-42)/148)**2
  blade.face([(x,c-6,sign*2.65),(x+8,c+3,sign*2.65),(x+16,c-6,sign*2.65),(x+8,c-2,sign*2.85)],4)
# Guard is an eight-point crystal star, set perpendicular to blade.
for i in range(16):
 a=i*pi/8;b=(i+1)*pi/8;r=22 if i%4==0 else 14 if i%2==0 else 7;s=22 if(i+1)%4==0 else 14 if(i+1)%2==0 else 7
 y,z=r*cos(a),r*sin(a);yy,zz=s*cos(b),s*sin(b)
 blade.face([(38,0,0),(38,y,z),(38,yy,zz)],0 if i%2 else 4)
 blade.face([(42,0,0),(42,yy,zz),(42,y,z)],1 if i%2 else 2)
 blade.face([(38,y,z),(42,y,z),(42,yy,zz),(38,yy,zz)],2)
blade.rod(0,35,4.0,4.4,1,8)
blade.rod(32,37,5,5,2,8);blade.rod(-3,0,6,4,2,8)
# White diamond wrap in eight grip segments; blue recesses between.
for x in [2,7,12,17,22,27]:
 for sign in [-1,1]:
  blade.face([(x,0,sign*4.6),(x+2.5,3.7,sign*4.6),(x+5,0,sign*4.6),(x+2.5,-3.7,sign*4.6)],3)
# Pommel blue crystal.
for i in range(6):
 a,b=i*pi/3,(i+1)*pi/3
 blade.face([(-9,0,0),(-3,5*cos(a),5*sin(a)),(-3,5*cos(b),5*sin(b))],4 if i%2 else 0)
blade.save('SM_FrostMoonBlade')
# Shallow moon, doubled base width after the user's in-game review. Total 480 cm.
wave=Obj()
for i in range(48):
 t0=-1+2*i/48;t1=-1+2*(i+1)/48
 def strip(t,v):
  y=t*240;front=50*(1-t*t);thickness=44*(1-abs(t)**1.4)
  return (front-thickness*v,y,0)
 wave.face([strip(t0,0),strip(t1,0),strip(t1,1),strip(t0,1)],0,[(0,(t0+1)/2),(0,(t1+1)/2),(1,(t1+1)/2),(1,(t0+1)/2)])
wave.save('SM_FrostMoonWave')
# Real sweep-shaped ribbon trails behind current blade, rotating with its swing.
trail=Obj()
for i in range(36):
 a0=-1.45*i/36;a1=-1.45*(i+1)/36
 def p(a,r):return(r*cos(a),r*sin(a),0)
 trail.face([p(a0,20),p(a0,190),p(a1,190),p(a1,20)],0,[(0,i/36),(1,i/36),(1,(i+1)/36),(0,(i+1)/36)])
trail.save('SM_FrostMoonSlash')
(W/'FrostMoon.mtl').write_text('\n'.join(f'newmtl Slot{i}\nKd .2 .65 .95\n'for i in range(5)))
(W/'model-report.json').write_text(json.dumps({'blade_triangles':len(blade.faces),'wave_triangles':len(wave.faces),'trail_triangles':len(trail.faces),'blade_length_cm':199,'blade_tip_x_cm':190,'wave_width_cm':480},indent=2))
print((W/'model-report.json').read_text())
