float2 p = (UV - .5) * 2;
if (Style > 2.5)
{
    bool teammate = Style > 3.5 && Style < 4.5;
    float2 q = abs(p);
    float d = teammate ? abs(q.x - .28) + abs(p.y) * 1.12 : abs(p.y - q.x * .79 + .17);
    float reach = 1 - smoothstep(.43, .52, q.x);
    float core = (1 - smoothstep(.06, .115, d)) * reach;
    float stroke = (1 - smoothstep(.12, .178, d)) * reach;
    float3 tint = teammate ? float3(1.0,.69,.31) : float3(.57,.91,1.08);
    return float4(lerp(float3(.026,.065,.15),tint,core), stroke * (Style > 4.5 ? 1 : .85));
}
float r = length(p);
float a = atan2(p.y, p.x);
float aa = max(.009, fwidth(r));
float halo = (1 - smoothstep(.036, .066, abs(r - .80))) * .38;
float ring = 1 - smoothstep(.021, .030 + aa, abs(r - .79));
float ringInk = 1 - smoothstep(.047, .055 + aa, abs(r - .79));
float2 k = abs(p);
float diamond = 1 - smoothstep(.060, .078, k.x * .80 + abs(k.y - .71));
float brackets = (1 - smoothstep(.008, .020 + aa, abs(k.x + k.y - .66))) * smoothstep(.38, .49, k.y);
bool ally = Style > .5 && Style < 1.5;
float emphasis = Style > 1.5 ? .98 : .89;
float visible = ally ? max(brackets * .89, diamond * .72) : max(ring * emphasis, diamond);
float3 color = ally ? float3(1.0, .73, .31) : lerp(float3(.02, .10, .22), float3(.78, 1.03, 1.14), max(ring, diamond));
float occlusion = 1 - smoothstep(25, 95, max(0, Pixel - Scene));
return float4(color, saturate(max(visible, ally ? 0 : max(halo * .3, ringInk * .65))) * occlusion);
