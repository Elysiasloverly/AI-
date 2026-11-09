float layer=floor(UV.x/2.0),u=frac(UV.x),v=UV.y;
float appear=smoothstep(0,.045,Age),expire=1-smoothstep(max(.0,Life-.18),Life,Age);
float ends=smoothstep(0,.035,v)*smoothstep(0,.035,1-v);
float edgeDistance=min(u,1-u);
float aa=max(fwidth(u),.002);
float whiteWidth=clamp(aa*OutlinePixels,.035,.30);
float outline=1-smoothstep(whiteWidth,whiteWidth+aa,edgeDistance);
float ink=1-smoothstep(0,max(.002,aa*InkPixels),edgeDistance);
float leading=1-smoothstep(.08,.35,u),flow=.5+.5*sin(v*42-Age*10+Seed);
float3 c=lerp(float3(.008,.055,.28),float3(.035,.42,.92),leading);
c=lerp(c,float3(1.35,1.43,1.48),outline);
// Thin blue ink outside the white stroke keeps the outline readable on pale ice.
c=lerp(c,float3(.004,.018,.055),ink*.92);
float alpha=ends*max(.80+.12*leading,outline*.99);
if(layer>.5){c=lerp(float3(.09,.40,.85),float3(.72,1.08,1.3),leading);alpha=ends*(1-smoothstep(.1,.8,u))*(.30+.08*flow);}
if(layer>1.5){c=float3(.75,1.1,1.3);alpha=(.5+.25*flow)*smoothstep(0,.05,v)*smoothstep(0,.05,1-v);}
return float4(c*Energy,alpha*appear*expire*Opacity);
