float2 p = (UV - .5) * 2;
float border = (1 - smoothstep(.28, .99, abs(p.x))) * (1 - smoothstep(.14, .97, abs(p.y)));
float curl = p.y + .22 * sin(p.x * 4 - T * .36) + .11 * sin(p.x * 9 + T * .25);
float first = 1 - smoothstep(.08, .51, abs(curl));
float second = 1 - smoothstep(.04, .32, abs(curl - .42 - .10 * sin(p.x * 11)));
float body = saturate(first * .70 + second * .28);
return float4(Tint.rgb, body * border * Opacity);
