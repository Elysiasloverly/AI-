float2 p = (UV - .5) * 2;
float r = length(p);
float envelope = (1 - smoothstep(.53, 1.0, r));
float2 q = p * float2(1.1, 1.0);
float ink = 0;
float light = 0;
if (Theme < .5)
{
    float spine = q.x + .10 * sin(q.y * 5);
    float bend = q.y + abs(spine) * 1.25;
    float leaves = abs(frac(bend * 5) - .5);
    ink = (1 - smoothstep(.11, .32, leaves)) * (1 - smoothstep(.17, .46, abs(spine))) * .85;
    ink = max(ink, (1 - smoothstep(.018, .054, abs(spine))) * .8);
    light = saturate(sin(20*q.x+3*q.y) * sin(11*q.y-4*q.x)) * .30;
}
else if (Theme < 1.5)
{
    float branch = q.y + .58 * q.x + .17 * sin(q.x * 2.7);
    ink = (1 - smoothstep(.022, .090, abs(branch))) * .64;
    for (int i = 0; i < 9; ++i)
    {
        float angle = i * 2.39996;
        float2 d = q - float2(cos(angle), sin(angle)) * (.16 + .079 * i);
        float2 petal = float2(dot(d, float2(cos(angle), sin(angle))), dot(d, float2(-sin(angle), cos(angle))));
        light = max(light, 1 - smoothstep(.71, 1, length(petal * float2(11, 18))));
    }
}
else if (Theme < 2.5)
{
    float fissure = q.x + .042 * sin(q.y * 27) + .022 * sin(q.y * 61);
    float d = abs(fissure);
    d = min(d, abs(fissure - (q.y + .31) * .71) + .67 * max(0., -.31 - q.y));
    d = min(d, abs(fissure + (q.y - .25) * .53) + .78 * max(0., q.y - .25));
    ink = 1 - smoothstep(.011, .035, d);
    light = (1 - smoothstep(.012, .035, abs(d - .047))) * .45;
    light = max(light, saturate(.48 + .38 * sin(q.y * 3 + q.x * 6 + sin(q.y * 7))) * .48);
}
else if (Theme < 3.5)
{
    float stem = q.x - q.y * .61 + .08 * sin(q.y * 5 + T * .55);
    float d = abs(frac((stem + .19) * 2.8) - .5);
    float joints = smoothstep(.04, .13, abs(frac(q.y * 3.2 + .2) - .5));
    ink = (1 - smoothstep(.030, .12, d)) * (.6 + .4 * joints);
    float blade = abs(frac((q.x + 1.4 * q.y) * 3.5) - .5);
    light = (1 - smoothstep(.03, .13, blade)) * (1 - smoothstep(.12, .30, abs(stem + .25)));
}
else
{
    float2 c = q + float2(.63, .38);
    float rr = length(c);
    float angle = atan2(c.y, c.x);
    float visibleArc = smoothstep(-.28, .10, c.x) * smoothstep(-.29, .13, c.y);
    float ticks = pow(saturate(cos(angle * 36)), 7) * smoothstep(.71, .74, rr) * (1 - smoothstep(.86, .90, rr));
    light = max(ticks, (1 - smoothstep(.010, .031, abs(rr - .94))) * .70) * visibleArc;
    ink = (1 - smoothstep(.017, .055, abs(rr - .67))) * visibleArc * .75;
    light = max(light, (1 - smoothstep(.018, .065, abs(abs(q.x - .12) + abs(q.y - .18) - .13))) * .48);
}
float mottled = .84 + .16 * sin(q.x * 27 + sin(q.y * 24)) * cos(q.y * 19);
return float4(lerp(Ink.rgb, Light.rgb, light / max(.01, light + ink)), max(ink, light) * envelope * mottled * Opacity);
