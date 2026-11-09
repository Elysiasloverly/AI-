// Authored for this project. UV-space ink shapes; no texture, scene capture or light.
float2 p=(UV-.5)*2;
float r=length(p),a=atan2(p.y,p.x),t=saturate(Progress);
float aa=clamp(fwidth(r),.008,.035),alpha=0;
float3 hue=Tint.rgb;
float warm=step(1.5,Style)*(1-step(2.5,Style))+step(6.5,Style);
float3 white=lerp(float3(.83,.94,1),float3(1,.91,.63),warm);
float heat=(1-smoothstep(.035,.24,t));
float noise=sin(p.x*18+p.y*11+Seed*6)*sin(p.y*14-p.x*9-t*4);
float2 q=p;
if(Shape<.5)
{
    if(Style<.5 || (Style>5.5 && Style<6.5))
    {
        // Compact four-ray star with a restrained second halo.
        float star=pow(saturate(.5+.5*cos(a*4)),20)*(1-smoothstep(.06,.91,r));
        float diagonal=pow(saturate(.5+.5*cos(a*4+3.14159)),32)*(1-smoothstep(.08,.53,r))*.3;
        alpha=max(star,diagonal)*.90;
    }
    else if(Style<1.5)
    {
        // Ice: sharp six-way fracture, split tips and a translucent inner crystal.
        float spokes=pow(saturate(.5+.5*cos(a*6)),28)*(1-smoothstep(.10,.94,r));
        float diamond=1-smoothstep(.25,.48,abs(p.x)*.75+abs(p.y)*1.25);
        float fracture=(1-smoothstep(.012,.04,abs(p.x+.08*sin(p.y*16))))*(1-smoothstep(.2,.6,abs(p.y)));
        alpha=max(spokes*.83,diamond*.45);alpha=max(alpha,fracture*.46);
    }
    else if(Style<2.5)
    {
        // Fire: curled petals surrounding a hot center, never a uniform opaque disc.
        float petals=.43+.09*sin(a*7-r*8-t*3)+.045*sin(a*11+t*4+Seed*3);
        float flame=1-smoothstep(petals-.16,petals+.10,r);
        float cavity=smoothstep(.025,.20,r);
        alpha=flame*(.60+.18*noise)*lerp(1,cavity,smoothstep(.08,.5,t)*.55);
        hue=lerp(float3(.48,.065,.028),Tint.rgb,saturate(1-r)*.9);
    }
    else if(Style<3.5)
    {
        // Frost bloom: airy six-petal snowflake and broken peripheral facets.
        float petals=pow(saturate(.5+.5*cos(a*6)),25)*(1-smoothstep(.09,.86,r));
        float tips=(1-smoothstep(.015,.06,abs(r-(.55+.07*cos(a*6)))))*smoothstep(.15,.65,cos(a*6));
        alpha=max(petals*.72,tips*.46)+(1-smoothstep(.15,.65,r))*.07;
    }
    else if(Style<4.5)
    {
        // Silver: one curved cut and a short counter-stroke.
        float outer=1-smoothstep(.66-aa,.75+aa,length(p+float2(.04,.04)));
        float inner=smoothstep(.53-aa,.62+aa,length(p-float2(.19,.12)));
        float cut=outer*inner*smoothstep(-.55,.28,p.y);
        float trail=(1-smoothstep(.012,.043,abs(p.y-.45*p.x+.12)))*(1-smoothstep(.18,.78,r));
        alpha=max(cut*.9,trail*.33);
    }
    else if(Style<5.5)
    {
        // Ofuda: torn frame, ink curl, and two tiny seal cuts.
        float frame=1-smoothstep(.016,.045,abs(max(abs(p.x)*1.28,abs(p.y))-.52));
        frame*=.72+.28*sin(p.y*27+Seed*5);
        float ink=(1-smoothstep(.018,.055,abs(p.x+.12*sin(p.y*10+t))))*(1-smoothstep(.28,.52,abs(p.y)));
        alpha=max(frame*.55,ink*.75);
        hue=lerp(float3(.055,.14,.21),Tint.rgb,.6);
    }
    else
    {
        // Continuous flame cast: three interleaved curls.
        float curl=abs(r-(.38+.13*sin(a*3+t*4)));
        alpha=(1-smoothstep(.02,.085,curl))*(1-smoothstep(.55,.86,r))*.8;
    }
    float core=exp2(-r*r*(FxKind>1.5?32:65));
    alpha=max(alpha,core*heat*.92);
    hue=lerp(hue,white,core*heat*.85);
    if(FxKind>1.5)
    {
        alpha=max(alpha,(1-smoothstep(.18,.72,r))*.055);
        hue=lerp(hue,white,heat*pow(saturate(1-r),3)*.45);
    }
}
else if(Shape<1.5)
{
    float ring=1-smoothstep(.012,.04+aa,abs(r-.73));
    float spokes=Style<3.5?6:4;
    float gaps=smoothstep(-.50,.15,cos(a*spokes+Seed*.6));
    float echo=(1-smoothstep(.008,.023+aa,abs(r-.54)))*smoothstep(.16,.37,t)*.25;
    float glints=pow(saturate(.5+.5*cos(a*spokes)),36)*(1-smoothstep(.035,.09,abs(r-.73)));
    alpha=ring*(.38+.36*gaps)+echo+glints*.30;
    if(FxKind<1.5)alpha*=.66;
    hue=lerp(hue,white,.26);
}
else
{
    if(Style>4.5 && Style<5.5)
    {
        float paper=1-smoothstep(.40,.54,max(abs(p.x),abs(p.y)*1.35));
        float notch=smoothstep(-.55,-.18,p.x+p.y*.25);
        alpha=paper*notch*.80;
        hue=lerp(float3(.15,.24,.30),Tint.rgb,.4);
    }
    else
    {
        float d=abs(p.x)*.70+abs(p.y)*1.5;
        alpha=(1-smoothstep(.36,.88,d))*.82;
        hue=lerp(hue,white,.48);
    }
}
float edge=1-smoothstep(.90,1.0,r);
return float4(hue*Energy,saturate(alpha*Opacity*edge));
