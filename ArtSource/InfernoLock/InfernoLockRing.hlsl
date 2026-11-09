float2 p=(UV-.5)*2;float r=length(p),a=atan2(p.y,p.x);
 float progress=saturate((T-Birth)/max(.1,Duration));float radius=.8*lerp(1,EndRatio,smoothstep(0,1,progress));
 // Screen-space lower bound preserves a clear stroke at the normal combat camera distance.
 float aa=max(fwidth(r),.004);float width=max(Width,aa*1.3);
 float axis=smoothstep(.96,.99,cos(a*4));float cut=1-axis*.85;
 float d=abs(r-radius);float main=(1-smoothstep(width,width+aa,d))*cut;
 float edge=(1-smoothstep(width+aa*1.3,width+aa*2.3,d))*cut;
 float tick=axis*smoothstep(radius+.035,radius+.045,r)*(1-smoothstep(radius+.12,radius+.13,r));
 float outer=(1-smoothstep(width*.8,width*.8+aa,abs(r-radius-.075)))*axis;
 float ink=max(main,max(tick,outer));float alpha=max(edge,ink)*Opacity;
 float3 tint=float3(Red,Green,Blue)*1.3;
 return float4(lerp(float3(.035,.055,.08),tint,saturate(ink)),alpha);