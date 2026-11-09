float side = P.x < 0.0 ? -1.0 : 1.0;
float anchor = 4.6;
float reach = max(0.0, abs(P.x)-anchor);
float mask = smoothstep(4,6,abs(P.x))*smoothstep(6.5,7.4,abs(P.y))*smoothstep(6,8,P.z)*(1-smoothstep(18,20,P.z));
float a = (.16 * sin(T * 4.35 + side*.42 + P.z*.11) + .09*sin(T*2.8)) * WingMotion;
return float3(side * reach * (cos(a)-1.0), reach * sin(a), .40 * reach * sin(a)) * mask;
