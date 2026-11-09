float2 p = (UV - .5) * 2;
float r = length(p);
float alpha = 0;
if (Theme < .5)
{
    // Uneven fern leaf silhouettes with a faint midrib.
    float y = p.y + .12 * sin(5 * p.x);
    float taper = 1 - saturate(abs(y));
    float leaves = abs(p.x + .13 * sin(y * 4)) + .21 * frac((abs(y) + abs(p.x) * .5) * 8);
    alpha = (1 - smoothstep(taper * .23, taper * .36 + .03, leaves)) * .82;
}
else if (Theme < 1.5)
{
    // Five scattered sakura petals and overlapping branch shadows.
    for (int k = 0; k < 7; ++k)
    {
        float a = k * 2.399963;
        float2 c = float2(cos(a), sin(a)) * (.14 + .087 * k);
        float2 d = p - c;
        float2 q = float2(dot(d, float2(cos(a), sin(a))), dot(d, float2(-sin(a), cos(a))));
        float petal = length(q * float2(5, 9)) + .14 * step(.10, q.x);
        alpha = max(alpha, (1 - smoothstep(.68, 1.05, petal)) * .83);
    }
    alpha = max(alpha, (1 - smoothstep(.045, .12, abs(p.y + .46 * p.x + .23))) * .34);
}
else if (Theme < 2.5)
{
    // Branching frost veins without a closed gameplay-like outline.
    float x = p.x + .052 * sin(p.y * 23) + .028 * sin(p.y * 61);
    float d = abs(x);
    d = min(d, abs(x - .62 * (p.y + .31)) + .6 * max(0, -.31 - p.y));
    d = min(d, abs(x + .45 * (p.y - .24)) + .6 * max(0, p.y - .24));
    alpha = (1 - smoothstep(.008, .034, d)) * .9;
}
else if (Theme < 3.5)
{
    // Long bamboo stems and narrow leaf shadows.
    float q = p.x + .46 * p.y;
    float stems = 1 - smoothstep(.035, .105, abs(frac((q + .11) * 2.3) - .5));
    float leaves = (1 - smoothstep(.035, .065, abs(frac((p.y + p.x * 1.27) * 4.1) - .5))) * (1 - smoothstep(.05, .24, abs(p.x + .19)));
    alpha = max(stems * .62, leaves * .85);
}
else
{
    // A partial clock-face detail, with gaps and no danger-ring shape.
    float2 q = p + float2(.52, .30);
    float a = atan2(q.y, q.x);
    float qr = length(q);
    float tick = pow(saturate(cos(a * 25)), 12) * smoothstep(.53, .61, qr) * (1 - smoothstep(.72, .77, qr));
    float arc = (1 - smoothstep(.007, .023, abs(qr - .79))) * smoothstep(-.14, .08, q.x) * smoothstep(-.38, -.23, q.y);
    alpha = max(tick * .95, arc * .55);
}
return float4(Tint.rgb, alpha * (1 - smoothstep(.65, 1, r)) * Opacity);
