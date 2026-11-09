float phase=T+Origin.x*.013+Origin.y*.019;float h=1-UV.y;float x=(UV.x-.5)*2;
float sway=.13*sin(h*7-phase*3.2)+.06*sin(h*12-phase*4.7);
float w=pow(saturate(1-h),.76)*.70;float d=abs(x-sway)-w;
float body=1-smoothstep(-.035,.08,d);float rimStroke=(1-smoothstep(.016,.075,abs(d+.05)))*body;
float core=(1-smoothstep(.035,.31,abs(x-sway)))*(1-smoothstep(.1,.48,h));
float cutoff=smoothstep(0,.07,h)*(1-smoothstep(.78,1,h));
float3 c=lerp(float3(.012,.07,.28),float3(.025,.43,1.0),.55+.45*(1-h));
c=lerp(c,float3(.20,.82,1.25),rimStroke*.7);c=lerp(c,float3(.61,.97,1.10),core*.82);
return float4(c*1.65,saturate(body*cutoff*Opacity*.80));
