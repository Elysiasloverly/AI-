float2 p=(UV-.5)*2;float r=length(p);float alpha=0;
if(Shape<.5)
{
 float a=abs(p.x+p.y*.55),b=abs(p.x-p.y*.55);
 float cut=1-smoothstep(.035,.075,min(a,b));
 float stem=1-smoothstep(.022,.055,abs(p.y));
 alpha=max(cut,stem*.7)*(1-smoothstep(.35,.92,r));
}
else if(Shape<1.5)
{
 float a=atan2(p.y,p.x);float ring=1-smoothstep(.012,.032,abs(r-(.45+Progress*.37)));
 alpha=ring*step(.4,sin(a*3+Seed*6))*.45;
}
else{float diamond=abs(p.x)*.55+abs(p.y)*1.7;alpha=1-smoothstep(.42,.60,diamond);}
float core=1-smoothstep(.05,.24,r);
float3 color=lerp(Tint,float3(.85,1.15,1.35),core*.85);
return float4(color*Energy,alpha*Opacity);
