float life=max(Lifetime,.001);
float a=Lifetime>.001 ? saturate((T-Birth)/life) : .35;
float envelope=smoothstep(0,.055,a)*(1-smoothstep(.70,1,a));
float angle=atan2(P.y,P.x);
float glint=pow(saturate(.5+.5*sin(angle*7-T*10+Phase)),8);
float energy=.70+.30*glint;
return float4(Tint.rgb*Energy*energy,saturate(Opacity*envelope));
