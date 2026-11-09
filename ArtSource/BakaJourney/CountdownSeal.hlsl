float2 p = (UV - .5) * 2;
float r = length(p);
float turn = frac(atan2(p.x, -p.y) / 6.2831853 + 1);
float sector = turn * 10;
float lit = saturate(Charge * 10 - floor(sector));
float band = (1 - smoothstep(.015, .030, abs(r - .76))) * smoothstep(.08, .19, frac(sector)) * (1 - smoothstep(.80, .92, frac(sector)));
float outline = (1 - smoothstep(.004, .019, abs(r - .59))) * .26;

// A paper talisman with a small sixfold frost stamp.
float2 q = float2(p.x * .96 + p.y * .28, p.y * .96 - p.x * .28);
float frame = max(abs(q.x) - .20, abs(q.y) - .39);
float edge = (1 - smoothstep(.008, .024, abs(frame))) * .78;
float fill = (1 - smoothstep(-.018, .008, frame)) * (.16 + Charge * .32);
float theta = atan2(q.y, q.x);
float stamp = pow(saturate(cos(theta * 6)), 13) * (1 - smoothstep(.10, .16, length(q))) * .82;
float alpha = max(band * (.23 + .77 * lit), max(outline, max(edge, max(fill, stamp))));
float3 color = lerp(float3(.33, .65, .84), float3(.79, .96, 1), saturate(.18 + lit * .78 + stamp));
return float4(color, alpha);
