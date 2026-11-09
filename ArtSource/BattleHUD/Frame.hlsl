float2 size=1/max(fwidth(UV),float2(.00001,.00001));
 float r=min(15,min(size.x,size.y)*.45);float2 q=abs((UV-.5)*size)-(size*.5-r);
 float d=length(max(q,0))+min(max(q.x,q.y),0)-r;float mask=saturate(.6-d);
 float edge=smoothstep(-1.6,-.2,d);float3 c=lerp(float3(.014,.024,.048),float3(.032,.052,.079),1-UV.y);
 c=lerp(c,float3(.24,.32,.38),edge*.5);
 float2 p=UV*size;float accent=(1-smoothstep(1.0,2.1,abs(p.y-1.8)))*smoothstep(18,25,p.x)*(1-smoothstep(min(size.x*.43,140),min(size.x*.43,140)+12,p.x));
 c=lerp(c,float3(.63,.12,.17),accent*.9);
 return float4(c,mask*.97);