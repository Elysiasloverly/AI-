float2 size=1/max(fwidth(UV),float2(.00001,.00001));
 float2 p=UV*size;float radius=min(24,min(size.x,size.y)*.42);
 float2 q=abs((UV-.5)*size)-(size*.5-radius-1.0);
 float d=length(max(q,0))+min(max(q.x,q.y),0)-radius;
 // Stable, very small pigment variation; no scrolling noise or moving edges.
 float fibre=sin(p.x*.37+p.y*.71)*sin(p.x*.83-p.y*.23);
 d+=fibre*.25;
 float feather=max(2,Feather);float body=1-smoothstep(-feather,2,d);
 float density=lerp(.96,.88,smoothstep(0,1,UV.y));
 if(Style>.5&&Style<1.5){
  float title=exp(-pow((UV.y-.27)*4.5,2));
  float track=exp(-pow((UV.y-.67)*5,2));
  density=.54+.42*max(title,track);
 }
 if(Style>1.5){float wash=exp(-pow((UV.x-.45)*1.4,4));density*=.88+.12*wash;}
 float3 paper=lerp(Bottom,Top,smoothstep(0,1,1-UV.y));
 paper+=fibre*.0025;
 // A faint short vermilion stroke replaces the long, hard top rule.
 float mark=exp(-pow((p.x-10.0)/1.0,2))*smoothstep(10,18,p.y)*(1-smoothstep(min(size.y*.48,37),min(size.y*.48,37)+7,p.y));
 paper=lerp(paper,Accent,mark*.20);
 float a=body*density*Opacity;
 float shadow=(1-body)*exp(-pow(max(d+1,0)/max(2,feather*.65),2))*.045;
 float alpha=a+shadow*(1-a);
 float3 c=(paper*a+float3(.04,.065,.08)*shadow*(1-a))/max(alpha,.0001);
 return float4(c,alpha);