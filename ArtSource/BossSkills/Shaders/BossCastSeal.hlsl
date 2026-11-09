float2 p = UV * 2 - 1;
float r = length(p);
float a = atan2(p.y, p.x);
float aa = clamp(fwidth(r), .005, .025);
float3 ink = float3(.98, .065, .018);
float3 pearl = float3(1, .96, .82);
float arms = 3;
if (Style > .5 && Style < 1.5) { ink = float3(.98, .018, .16); pearl = float3(1, .84, .95); arms = 8; }
else if (Style < 2.5 && Style > 1.5) { ink = float3(.91, .015, .20); pearl = float3(1, .87, .93); arms = 14; }
else if (Style < 3.5 && Style > 2.5) { ink = float3(1, .075, .008); arms = 3; }
else if (Style < 4.5 && Style > 3.5) { ink = float3(.035, .31, 1); pearl = float3(.80, .98, 1); arms = 6; }
else if (Style < 5.5 && Style > 4.5) { ink = float3(.018, .94, .27); pearl = float3(.80, 1, .87); arms = 6; }
else if (Style < 6.5 && Style > 5.5) { ink = float3(1, .43, .008); pearl = float3(1, .96, .70); arms = 4; }
else if (Style < 7.5 && Style > 6.5) { ink = float3(1, .032, .009); arms = 7; }
else if (Style > 7.5) { ink = float3(.98, .008, .055); pearl = float3(1, .95, .94); arms = 3; }

if (Stage > .5)
{
    // Open upright mandala. Its central hole reveals the character silhouette.
    float turn = a + EffectAge * .29;
    float ring = 1 - smoothstep(.011, .011 + aa, abs(r - .88));
    float inkRim = 1 - smoothstep(.038, .038 + aa, abs(r - .88));
    float inner = (1 - smoothstep(.010, .030, abs(r - .69))) * smoothstep(.1, .55, cos(turn * arms));
    float gates = (1 - smoothstep(.025, .048, abs(r - .79))) * pow(saturate(.5 + .5 * cos(turn * arms)), 15);
    float petals = 1 - smoothstep(.011, .032, abs(r - (.58 + .11 * cos(a * arms - EffectAge * .8))));
    float outer = (1 - smoothstep(.007, .025, abs(r - .96))) * smoothstep(.25, .65, cos(a * arms * 2 + EffectAge));
    float flare = pow(saturate(.5 + .5 * cos(a * arms - EffectAge * .24)), 24)
        * smoothstep(.49, .65, r) * (1 - smoothstep(.87, .99, r));
    float detail = max(petals * .68, max(inner * .73, max(gates, max(outer * .70, flare))));
    float light = saturate(max(ring, detail) * (.76 + Pulse * .35));
    float3 color = lerp(float3(.018, .003, .014), ink * 2.3, detail);
    color = lerp(color, pearl * (3.1 + Pulse * 2.2), saturate(ring + gates * .64 + flare * .83));
    float alpha = max(inkRim * .58, light * .90) * smoothstep(.39, .47, r);
    return float4(color, saturate(alpha * Opacity));
}

float frame = 1 - smoothstep(.013, .013 + aa, abs(r - .884));
float underpaint = 1 - smoothstep(.052, .052 + aa, abs(r - .884));
float index = (1 - smoothstep(.024, .047, abs(r - .79))) * smoothstep(.42, .67, cos(a * 16 - EffectAge * .7));
float pattern = 0;
float slash = 0;
if (Style > .5 && Style < 1.5)
{
    float spokes = (1 - smoothstep(.024, .060, abs(sin(a * 4)))) * smoothstep(.24, .37, r) * (1 - smoothstep(.86, .96, r));
    float sector = a - floor((a + .3927) / .785398) * .785398;
    float heads = 1 - smoothstep(.034, .069, abs(r - .72) + abs(sector) * .8);
    pattern = max(spokes * (.72 + Pulse * .28), heads);
}
else if (Style > 1.5 && Style < 2.5)
{
    float wave = 1 - smoothstep(.012, .034, abs(r - lerp(.23, .86, frac(EffectAge * 2))));
    float beads = (1 - smoothstep(.030, .057, abs(r - .68))) * pow(saturate(.5 + .5 * cos(a * 14)), 13);
    pattern = max(wave * .88, beads * (.64 + Pulse * .36));
}
else if (Style > 2.5 && Style < 3.5)
{
    float direction = min(abs(a), min(abs(a - Spread), abs(a + Spread)));
    float guide = (1 - smoothstep(.011, .029, direction)) * smoothstep(.18, .27, r) * (1 - smoothstep(.89, .99, r));
    float blade = 1 - smoothstep(.029, .058, abs(r - .72) + direction * 1.45);
    slash = max(guide * .85, blade);
    frame *= .63;
    underpaint *= .71;
}
else if (Style > 3.5 && Style < 4.5)
{
    float frost = (1 - smoothstep(.014, .037, abs(sin(a * 3)))) * smoothstep(.20, .32, r) * (1 - smoothstep(.75, .84, r));
    float facets = 1 - smoothstep(.013, .034, abs(r - (.52 + .12 * cos(a * 6))));
    pattern = max(frost * .86, facets);
}
else if (Style > 4.5 && Style < 5.5)
{
    float blossom = 1 - smoothstep(.011, .035, abs(r - (.49 + .22 * cos(a * 6 + EffectAge))));
    float second = 1 - smoothstep(.009, .028, abs(r - (.39 + .13 * cos(a * 6 - EffectAge))));
    pattern = max(blossom, second * .73);
}
else if (Style > 5.5 && Style < 6.5)
{
    float diamond = abs(p.x) + abs(p.y);
    pattern = max(1 - smoothstep(.010, .035, abs(diamond - .73)), 1 - smoothstep(.009, .030, abs(diamond - .48)));
}
else if (Style > 6.5 && Style < 7.5)
{
    float embers = pow(saturate(.5 + .5 * cos(a * 7 + EffectAge)), 12);
    float fire = (1 - smoothstep(.019, .043, abs(r - (.48 + .17 * sin(a * 4 + EffectAge * 3))))) * embers;
    pattern = max(fire, 1 - smoothstep(.010, .030, abs(r - .49)));
}
else
{
    pattern = 1 - smoothstep(.013, .038, abs(r - (.51 + .13 * cos(a * 3 + EffectAge))));
}
float shock = (1 - smoothstep(.012, .038, abs(r - lerp(.36, .95, 1 - Pulse)))) * Pulse;
float lit = saturate(max(shock, max(slash, max(frame, max(index * .76, pattern)))) * (.94 + Pulse * .24));
float3 color = lerp(float3(.018, .003, .014), ink * 2.1, lit);
color = lerp(color, pearl * 3.5, saturate(frame * .92 + pattern * .35 + slash * .67 + shock));
float alpha = max(underpaint * .88, lit * .93);
return float4(color, saturate(alpha * Opacity));
