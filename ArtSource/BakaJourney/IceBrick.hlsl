float2 p = UV * 2.0 - 1.0;
float rim = abs(p.x) + .76 * abs(p.y + .10);
float body = 1.0 - smoothstep(.68,.73,rim);
float top = smoothstep(-.40,-.27,p.y + .48 * abs(p.x));
float side = smoothstep(-.01,.01,p.x);
float facetEdge = exp(-110.0 * abs(rim - .66)) + exp(-55.0 * abs(p.x)) * (1.0 - top);
float diagonal = exp(-44.0 * abs(p.y + .04 - p.x*.56)) * .24;
float3 tone = lerp(lerp(float3(.11,.42,.68),float3(.26,.73,.89),side),float3(.56,.89,.99),top);
float glint = exp(-18.0 * length(p - float2(-.25,-.39)));
return float4(saturate(tone + (facetEdge*.45 + diagonal + glint)*float3(.56,.65,.70)), body);

