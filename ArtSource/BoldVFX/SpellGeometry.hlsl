// Minimal spell geometry: solid color, sharp silhouettes, sparse negative space.
float2 p=(UV-.5)*2;
float r=length(p),a=atan2(p.y,p.x),t=saturate(Progress);
float aa=clamp(fwidth(r),.009,.035),alpha=0;
float3 cold=float3(.94,.99,1),warm=float3(1,.94,.68);
float fire=step(1.5,Style)*(1-step(2.5,Style))+step(6.5,Style);
float3 white=lerp(cold,warm,fire),hue=Tint.rgb;
float hot=1-smoothstep(.05,.30,t);
if(Shape<.5)
{
 if(Style<.5 || (Style>5.5 && Style<6.5))
 {
  float ray=pow(abs(cos(a*2)),18);
  float border=.19+.70*ray;
  alpha=(1-smoothstep(border-aa,border+aa,r));
  float secondary=pow(abs(sin(a*2)),36)*(1-smoothstep(.14,.45,r));
  alpha=max(alpha,secondary*.85);
  hue=lerp(Tint.rgb,white,saturate(1-r*1.1)*.9);
 }
 else if(Style<1.5 || (Style>2.5 && Style<3.5))
 {
  float six=pow(saturate(.5+.5*cos(a*6)),18);
  float ray=(1-smoothstep(.10+.70*six-aa,.10+.70*six+aa,r));
  float diamond=1-smoothstep(.34-aa,.34+aa,abs(p.x)*.9+abs(p.y)*1.4);
  float branch=pow(saturate(.5+.5*cos(a*6+.65)),32)*(1-smoothstep(.3,.65,r));
  alpha=max(ray,diamond);alpha=max(alpha,branch*.85);
  float ring=1-smoothstep(aa,aa+.018,abs(r-.62));
  alpha=max(alpha,ring*smoothstep(.1,.5,t)*.75);
  hue=lerp(float3(.015,.35,.95),white,saturate(.90-r*.65));
 }
 else if(Style<2.5)
 {
  float petals=.23+.49*pow(saturate(.5+.5*cos(a*6+t*.7)),5);
  float bloom=1-smoothstep(petals-aa,petals+aa,r);
  float inner=1-smoothstep(.19-aa,.19+aa,r);
  alpha=max(bloom*.93,inner);
  hue=lerp(float3(1,.19,.025),white,inner*.9+hot*saturate(1-r*2.5)*.3);
 }
 else if(Style<4.5)
 {
  float outer=1-smoothstep(.77-aa,.77+aa,length(p+float2(.02,.02)));
  float inner=smoothstep(.59-aa,.59+aa,length(p-float2(.15,.12)));
  float arc=outer*inner*smoothstep(-.4,-.1,p.y);
  float slash=(1-smoothstep(.018,.035,abs(p.y-.55*p.x+.08)))*(1-smoothstep(.62,.88,r));
  alpha=max(arc,slash*.96);hue=lerp(float3(.2,.65,1),white,.85);
 }
 else if(Style<5.5)
 {
  float frame=1-smoothstep(.018,.04,abs(max(abs(p.x)*1.2,abs(p.y))-.55));
  float stem=(1-smoothstep(.035,.055,abs(p.x)))*(1-smoothstep(.35,.38,abs(p.y)));
  float bars=(1-smoothstep(.028,.045,abs(abs(p.y)-.19)))*(1-smoothstep(.14,.18,abs(p.x)));
  alpha=max(frame,max(stem,bars));
  hue=lerp(float3(.95,.06,.16),white,frame*.85);
 }
 else
 {
  float angle=a+t*1.4;
  float spokes=pow(saturate(.5+.5*cos(angle*3)),5);
  float swirl=1-smoothstep(.035,.065,abs(r-(.30+.29*spokes)));
  float core=1-smoothstep(.13,.16,r);
  alpha=max(swirl,core);hue=lerp(float3(1,.22,.02),white,core*.9);
 }
 float pin=1-smoothstep(.07,.12,r);
 alpha=max(alpha,pin*hot);hue=lerp(hue,white,pin*.9);
}
else if(Shape<1.5)
{
 float arc=1-smoothstep(.014,.035+aa,abs(r-.74));
 float segments=smoothstep(-.45,-.2,cos(a*(fire>0?6:8)+Seed*.5));
 float inner=1-smoothstep(.009,.025+aa,abs(r-.54));
 float ticks=pow(saturate(.5+.5*cos(a*(fire>0?6:8))),24)*(1-smoothstep(.05,.075,abs(r-.74)));
 alpha=max(arc*segments,max(ticks,inner*.55*smoothstep(.12,.40,t)));
 hue=lerp(Tint.rgb,white,.45);
}
else
{
 float d=Style>4.5&&Style<5.5?max(abs(p.x),abs(p.y)*1.35):abs(p.x)*.65+abs(p.y)*1.65;
 alpha=1-smoothstep(.48,.53,d);
 hue=lerp(Tint.rgb,white,.8);
}
float edge=1-smoothstep(.94,1,r);
return float4(hue*Energy,saturate(alpha*Opacity*edge));
