float period = frac((T + Phase)*.263);
float blink = 1.0 - smoothstep(.008,.041,abs(period-.25));
blink = max(blink, (1.0 - smoothstep(.005,.026,abs(period-.33))) * step(.55,frac(floor((T+Phase)*.263)*.618)));
blink = lerp(blink, saturate(BlinkValue), step(.5, BlinkDriver));
float eye = step(5.8,P.y) * step(1.1,abs(P.x)) * step(abs(P.x),8.5) * step(24.0,P.z) * step(P.z,31.8);
return float3(0,0,(27.0-P.z)*blink*eye*BlinkDepth);
