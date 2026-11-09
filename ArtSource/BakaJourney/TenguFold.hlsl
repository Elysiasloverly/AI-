float side = P.y < 0.0 ? -1.0 : 1.0;
float distanceFromRoot = max(0.0,abs(P.y)-14.0);
float a = Fold * (.66 + side*.11) * smoothstep(2.0,42.0,distanceFromRoot);
return float3(-distanceFromRoot*sin(a), side*distanceFromRoot*(cos(a)-1.0), -distanceFromRoot*sin(a)*.24);
