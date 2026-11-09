float3 p = P - Origin;
float phase = (p.z + .33 * p.x + .19 * p.y) * .058 - T * 1.45;
float band = pow(saturate(.5 + .5 * sin(phase)), 10);
float rim = saturate(Edge);
float warmth = saturate(.67 * rim + .40 * band);
float3 color = lerp(Lacquer.rgb, Gold.rgb, warmth) * (.65 + 1.7 * rim + .45 * band);
float alpha = saturate((.13 + .49 * rim + .18 * band) * Strength);
return float4(color, alpha);
