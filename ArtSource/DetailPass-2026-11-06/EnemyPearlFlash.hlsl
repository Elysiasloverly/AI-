float age=max(0,T-Start);float fade=1-smoothstep(Hold,Hold+max(.001,Fade),age);
float rim=pow(saturate(Edge),.72);float coverage=lerp(.30,.96,rim);
float3 c=lerp(FlashColor.rgb,float3(.76,.94,1.04),rim*.45)*Intensity;
return float4(c,saturate(coverage*Opacity*fade));
