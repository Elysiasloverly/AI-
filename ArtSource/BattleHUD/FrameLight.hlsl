float2 size=1/max(fwidth(UV),float2(.00001,.00001));
 float r=min(15,min(size.x,size.y)*.45);float2 q=abs((UV-.5)*size)-(size*.5-r);
 float d=length(max(q,0))+min(max(q.x,q.y),0)-r;float mask=saturate(.6-d);
 float edge=smoothstep(-1.6,-.2,d);float3 c=lerp(Bottom,Top,1-UV.y);
 c=lerp(c,Edge,edge*.24);
 float2 p=UV*size;float accent=(1-smoothstep(1.0,2.1,abs(p.y-1.8)))*smoothstep(18,25,p.x)*(1-smoothstep(min(size.x*.43,140),min(size.x*.43,140)+12,p.x));
 c=lerp(c,Accent,accent*.8);return float4(c,mask*Alpha);