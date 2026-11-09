float2 p=(UV-.5)*2;float x=p.x,y=p.y;float a=0;
 if(Style<.5){float petals=1-smoothstep(.35,.43,min(length(p-float2(.23,0)),min(length(p+float2(.23,0)),min(length(p-float2(0,.23)),length(p+float2(0,.23))))));float cut=1-smoothstep(.10,.16,length(p));a=max(petals*.8,cut);}
 else if(Style<1.5){float outer=max(max(abs(x)-.60,-y-.66),max(y-.73,y+abs(x)*1.1-.73));float inner=max(max(abs(x)-.40,-y-.44),max(y-.49,y+abs(x)*1.1-.49));a=(1-smoothstep(-.04,.04,outer))*smoothstep(-.035,.035,inner);}
 else if(Style<2.5){float d=abs(x)*.75+abs(y)*1.2;float star=1-smoothstep(.52,.62,d);float core=1-smoothstep(.14,.22,length(p));a=max(star*.85,core);}
 else if(Style<3.5){float c1=abs(x+.28+abs(y)*.60);float c2=abs(x-.34+abs(y)*.60);a=(1-smoothstep(.08,.17,min(c1,c2)))*(1-smoothstep(.52,.65,abs(y)));}
 else if(Style<4.5){float ring=1-smoothstep(.055,.12,abs(length(p)-.58));float diamond=1-smoothstep(.05,.11,abs(abs(x)+abs(y)-.60));float cross=(1-smoothstep(.04,.09,min(abs(x),abs(y))))*(1-smoothstep(.75,.84,length(p)));a=max(max(ring*.75,diamond),cross*.65);}
 else{float ring=1-smoothstep(.07,.14,abs(length(p)-.60));float square=1-smoothstep(.035,.08,abs(max(abs(x),abs(y))-.18));a=max(ring,square);}
 return float4(1,1,1,saturate(a));