float2 size=1/max(fwidth(UV),float2(.00001,.00001));
 float r=min(size.y*.5,size.x*.5);float2 q=abs((UV-.5)*size)-(size*.5-r);
 float d=length(max(q,0))+min(max(q.x,q.y),0)-r;float a=saturate(.6-d);
 float v=lerp(.72,1.13,1-UV.y);v+=.16*exp(-pow((UV.y-.22)*9,2));
 return float4(v,v,v,a);