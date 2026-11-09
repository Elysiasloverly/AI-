float2 p = UV * 2.0 - 1.0;
float r = length(p);
float mist = 0.0;
for (int i = 0; i < 4; ++i)
{
    float k = float(i);
    float2 q = p - float2((k - 1.5) * .32, .17*sin(k*6.1+Seed*2.0));
    mist += exp(-4.6 * dot(q * float2(1.0,1.65),q * float2(1.0,1.65))) * (.43 + .12*sin(k*2.7));
}
mist *= 1.0 - smoothstep(.62, .99, r);
float crystal = 1.0 - smoothstep(.48,.62,abs(p.x)*.86 + 1.15*abs(p.y-.12*p.x));
float facet = saturate(.7 + p.y*.50 - p.x*.24);
float dash = exp(-28.0*abs(p.y + .08*sin(p.x*6.1))) * smoothstep(-1.0,-.44,p.x) * (1.0-smoothstep(.42,.93,p.x));
float arc = exp(-20.0*abs(r-.62))*(.36+.44*saturate(abs(p.x)*1.7));
float core = Style < .5 ? mist + dash*.32 : (.39+Style*.065)*crystal + mist*.28;
float mask = Shape < .5 ? core : (Shape < 1.5 ? arc : crystal * facet);
float3 ice = lerp(Tint.rgb, float3(.85, .98, 1.0), .18 + .36*facet);
float brightness = (Shape > 1.5 ? 1.25 : .92) * (1.0 - Progress*.22);
return float4(ice*brightness, saturate(mask * Opacity * (Shape > 1.5 ? 1.7 : 1.2)));
