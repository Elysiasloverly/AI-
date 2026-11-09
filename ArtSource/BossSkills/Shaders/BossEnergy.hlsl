float2 p = UV * 2 - 1;
float facing = abs(dot(normalize(Normal), normalize(ViewDirection)));
float rim = pow(saturate(1 - facing), 1.8);
float3 charcoal = float3(.038, .004, .018);
float3 color = Ink.rgb;
float alpha = 1;

if (Style < .5)
{
    // Dark solar crust, white-hot fractures and broad moving scarlet plates.
    float2 grid = UV * float2(11, 7) + float2(UV.y * .55, EffectAge * .09);
    float2 cell = frac(grid) - .5;
    float plateEdge = max(abs(cell.x) * .866 + abs(cell.y) * .5, abs(cell.y));
    float fracture = 1 - smoothstep(.015, .055, abs(plateEdge - .425));
    float branches = pow(saturate(.5 + .5 * sin(UV.y * 47 + UV.x * 21 + EffectAge * 6)), 15);
    float plates = frac(sin(dot(floor(grid), float2(73.17, 31.91))) * 1719.13);
    color = lerp(charcoal, Ink.rgb * 2.05, .28 + plates * .58);
    color = lerp(color, Tip.rgb * 5.2, saturate(fracture + branches * .30 + rim * .18 + Flash * .32));
}
else if (Style < 1.5)
{
    // Axial streams remain visible along the whole map-length beam.
    float channel = pow(saturate(.5 + .5 * sin(UV.x * 25 + UV.y * 103 - EffectAge * 37)), 5);
    float cuts = pow(saturate(.5 + .5 * sin(UV.y * 183 - EffectAge * 43)), 20);
    color = lerp(Ink.rgb * 2.4, Tip.rgb * 7.5, saturate(.63 + channel * .33 + cuts * .17 + Flash * .15));
    alpha = .96;
}
else if (Style < 2.5)
{
    float braid = pow(saturate(.5 + .5 * sin(UV.x * 41 - UV.y * 63 + EffectAge * 25)), 7);
    float cross = pow(saturate(.5 + .5 * cos(UV.x * 39 + UV.y * 53 - EffectAge * 29)), 10);
    color = lerp(Ink.rgb * 2.3, Tip.rgb * 4.8, saturate(braid * .61 + cross * .3 + Flash * .4));
    alpha = .12 + rim * .59 + braid * .30 + cross * .12;
}
else if (Style < 3.5)
{
    // Ink-backed crescent with a narrow luminous cutting edge.
    float y = abs(p.y);
    float center = .46 - .84 * pow(y, 1.62);
    float thickness = lerp(.25, .047, pow(y, 1.3));
    float d = center - p.x;
    float ends = 1 - smoothstep(.905, .994, y);
    float edge = (1 - smoothstep(.007, .023, abs(d))) * ends;
    float backing = smoothstep(-.085, -.049, d) * (1 - smoothstep(thickness, thickness + .081, d)) * ends;
    float hot = smoothstep(-.009, .016, d) * (1 - smoothstep(thickness * .60, thickness, d));
    float streaks = pow(saturate(.5 + .5 * cos(y * 51 - EffectAge * 24 + d * 37)), 14);
    color = lerp(charcoal, Ink.rgb * 2.15, hot * .92);
    color = lerp(color, Tip.rgb * 2.0, saturate(edge + streaks * hot * .10 + Flash * .2));
    alpha = max(backing * .98, edge);
}
else if (Style < 4.5)
{
    float facet = .5 + .5 * cos(floor(UV.x * 7) * 2.39996);
    float fracture = pow(saturate(.5 + .5 * cos(p.y * 27 + p.x * 19 + sin(p.x * 11) * 1.4)), 16);
    float edge = saturate(rim * .73 + fracture * .75 + Flash * .6);
    color = lerp(charcoal + Ink.rgb * .16, Ink.rgb * 1.8, facet);
    color = lerp(color, Tip.rgb * 4.3, edge);
    alpha = .99;
}
else if (Style < 5.5)
{
    // An open cage of interlocked gold diamonds, with room to see the Boss.
    float a = abs(sin((UV.x * 8 + UV.y * 5 + EffectAge * .27) * 3.14159));
    float b = abs(sin((UV.x * 8 - UV.y * 5 - EffectAge * .27) * 3.14159));
    float lattice = 1 - smoothstep(.035, .12, min(a, b));
    float nodes = (1 - smoothstep(.06, .19, a)) * (1 - smoothstep(.06, .19, b));
    color = lerp(Ink.rgb * 1.9, Tip.rgb * 4.8, saturate(nodes * .82 + lattice * .39 + rim * .45));
    alpha = .012 + lattice * .70 + rim * .23 + nodes * .20;
}
else if (Style < 6.5)
{
    // Broken flame ribbons keep the fire volume from reading as a solid cone.
    float cells = pow(saturate(.5 + .5 * sin(UV.x * 21 + UV.y * 17 - EffectAge * 19)), 5);
    float tongues = pow(saturate(.5 + .5 * sin(UV.x * 37 - UV.y * 23 + EffectAge * 24)), 10);
    float breaks = smoothstep(.11, .46, .5 + .5 * sin(UV.y * 39 + UV.x * 8 - EffectAge * 15));
    color = lerp(Ink.rgb * 1.7, Tip.rgb * 4.5, saturate(cells * .68 + tongues * .7 + Flash * .5));
    alpha = (.035 + cells * .64 + tongues * .53) * breaks * (Phase < .5 ? .96 : .77);
}
else if (Style < 7.5)
{
    float seals = pow(saturate(.5 + .5 * cos(UV.x * 38 + EffectAge * 4)), 17);
    float rings = pow(saturate(.5 + .5 * cos(UV.y * 49 - EffectAge * 9)), 13);
    color = lerp(Ink.rgb * 2.1, Tip.rgb * 4.2, saturate(seals * .59 + rings * .46 + Flash * .5));
    alpha = .015 + rim * .27 + seals * rings * .88;
}
else if (Style < 8.5)
{
    float splinters = pow(saturate(.5 + .5 * cos(UV.x * 39 + UV.y * 27 - EffectAge * 11)), 11);
    color = lerp(Ink.rgb * 2.2, Tip.rgb * 5.6, saturate(rim * .64 + splinters * .47 + Flash * .6));
    alpha = (.13 + rim * .68 + splinters * .36) * (.65 + Progress * .35);
}
else if (Style < 9.5)
{
    float streams = pow(saturate(.5 + .5 * cos(UV.x * 49 + UV.y * 9 - EffectAge * 3)), 14);
    float rise = pow(saturate(.5 + .5 * cos(UV.y * 27 + EffectAge * 12 + UV.x * 4)), 8);
    float petals = pow(saturate(.5 + .5 * sin(UV.x * 19 - UV.y * 11 + EffectAge * 5)), 5);
    color = lerp(Ink.rgb * 1.6, Tip.rgb * 4.2, saturate(streams * rise + rim * .54 + petals * .2));
    alpha = .015 + streams * rise * .93 + rim * .18 + petals * .10;
}
else if (Style < 10.5)
{
    // Solar prominences around rolling spheres, with a hollow transparent interior.
    float arcs = pow(saturate(.5 + .5 * sin(UV.x * 31 + UV.y * 17 - EffectAge * 13)), 8);
    float sparks = pow(saturate(.5 + .5 * cos(UV.x * 79 - UV.y * 47 + EffectAge * 19)), 20);
    color = lerp(Ink.rgb * 2.6, Tip.rgb * 5.7, saturate(arcs * .70 + sparks * .8 + Flash * .25));
    alpha = rim * (.22 + arcs * .8) + sparks * .12;
}
else if (Style < 11.5)
{
    float r = length(p);
    float a = atan2(p.y, p.x);
    float band = 1 - smoothstep(.010, .034, abs(r - .72));
    float outer = (1 - smoothstep(.009, .027, abs(r - .91))) * smoothstep(.18, .58, cos(a * 12 - EffectAge * 3));
    float inner = 1 - smoothstep(.012, .033, abs(r - .47));
    float tips = pow(saturate(.5 + .5 * cos(a * 4 + EffectAge * .8)), 26) * (1 - smoothstep(.2, .98, r));
    float lines = max(band, max(outer, max(inner * .72, tips * .92)));
    color = lerp(Ink.rgb * 2.1, Tip.rgb * 6.6, saturate(band * .9 + tips * .72 + Flash * .3));
    alpha = lines * (1 - smoothstep(.97, 1, r));
}
else if (Style < 12.5)
{
    float meridians = pow(saturate(.5 + .5 * cos(UV.x * 31 - UV.y * 7 + EffectAge * 2)), 18);
    float spirals = pow(saturate(.5 + .5 * sin(UV.y * 39 + UV.x * 13 - EffectAge * 8)), 12);
    color = lerp(Ink.rgb * 1.7, Tip.rgb * 4.3, saturate(meridians * .66 + spirals * .46 + rim * .5));
    alpha = .008 + meridians * spirals * .72 + rim * .32;
}
else
{
    // A few sharp chips follow each crescent instead of another opaque blade.
    float row = floor((p.y + 1) * 7);
    float seed = frac(sin(row * 79.37 + 3.1) * 3117.23);
    float y = frac((p.y + 1) * 7) - .5;
    float x = p.x + .35 - seed * .55;
    float shard = 1 - smoothstep(.07, .16, abs(x) + abs(y) * .58);
    float cut = 1 - smoothstep(.032, .075, abs(x) + abs(y) * .38);
    color = lerp(Ink.rgb * 2.6, Tip.rgb * 5.9, cut);
    alpha = shard * (1 - smoothstep(.83, 1, abs(p.y))) * (.64 + .36 * sin(EffectAge * 7 + seed * 3));
}
return float4(color, saturate(alpha * Opacity));
