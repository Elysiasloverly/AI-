float2 p = UV * 2.0 - 1.0;
float radius = length(p);
float angle = atan2(p.y, p.x);
float aa = clamp(fwidth(radius) * .9, .007, .027);
float3 soot = float3(.018, .002, .011);
float3 color = Ink.rgb;
float alpha = 0;

// Styles 0/1/2 are radius-accurate warnings, including their impact flash.
if (Style < 2.5)
{
    if (Phase < .5)
    {
        float border = 1 - smoothstep(.035, .035 + aa, abs(radius - .939));
        float bright = 1 - smoothstep(.012, .012 + aa, abs(radius - .939));
        float edge = 1 - smoothstep(.972, .991, radius);
        float tick = (1 - smoothstep(.022, .040, abs(radius - .817))) * smoothstep(.51,.69,cos(angle * (Style < .5 ? 12 : 8)));
        float normalizedAngle = frac((angle + 1.5708) / 6.2831853 + 1);
        float progressArc = (1 - smoothstep(.013, .013 + aa, abs(radius - .876))) * step(normalizedAngle, Progress);
        float inward = 1 - smoothstep(.009,.024,abs(radius - lerp(.87,.12,Progress)));
        float cross = 1 - smoothstep(.01,.033,min(abs(p.x),abs(p.y)));
        float center = cross * (1 - smoothstep(.13,.18,radius));
        float motif = tick;
        if (Style > .5 && Style < 1.5)
        {
            float shard = (1 - smoothstep(.015,.031,abs(sin(angle * 3)))) * smoothstep(.13,.27,radius) * (1 - smoothstep(.65,.78,radius));
            motif = max(motif, shard * (.52 + .36 * Progress));
        }
        if (Style > 1.5) motif = max(motif, (1 - smoothstep(.014,.035,abs(radius - .65))) * smoothstep(.47,.68,cos(angle*3 - EffectAge*2)));
        float lit = saturate(max(max(bright,progressArc),max(max(center,motif),inward*.55)));
        alpha = max(border * 1.0, max(lit * .92, edge * (.17 + .15 * Progress)));
        color = lerp(lerp(soot, Ink.rgb * .51, edge * .48), lerp(Ink.rgb * 2.8, Tip.rgb * 3.5, saturate(bright * .72 + motif * .45 + progressArc * .52)), lit);
    }
    else
    {
        float wave = 1 - smoothstep(.018,.050,abs(radius - lerp(.21,.97,Progress)));
        float echo = 1 - smoothstep(.014,.045,abs(radius - lerp(.12,.83,Progress)));
        float rays = pow(saturate(.5+.5*cos(angle*12+.4*sin(angle*9))),18) * (1-smoothstep(.2,.87,radius));
        alpha = max(max(wave,echo*.46),rays*.8);
        color = lerp(Ink.rgb * 2.15, Tip.rgb * 5.2, saturate(wave*.8 + rays*.3));
    }
}
else if (Style < 3.5)
{
    if (Phase > 1.5)
    {
        // Slim incandescent fragments ride behind the full-size meteor.
        float row = clamp(floor(UV.y*8.0),0.0,7.0);
        float seed = frac(sin((row+3)*71.117)*3919.7);
        float y = frac(UV.y*8.0) - .5;
        float x = UV.x;
        float head = frac(seed + EffectAge*(1.47+seed*.56));
        float streak = saturate(1-abs((x-head)*3.4));
        float ridge = (1-smoothstep(.04,.19,abs(y))) * streak * streak;
        float thin = (1-smoothstep(.026,.092,abs(y))) * smoothstep(.34,.93,streak);
        float trim = smoothstep(.08,.24,x) * (1-smoothstep(.82,.99,x));
        color = lerp(Ink.rgb*1.63,Tip.rgb*5.1,saturate(thin*.83));
        return float4(color,saturate((ridge*.68+thin*.53)*trim*Opacity));
    }
    // Ember bed. The dark lip remains visible under exposure on white maps.
    float mask = 1 - smoothstep(.94,.989,radius);
    float edge = 1 - smoothstep(.017,.041,abs(radius-.93));
    float curl = .60 + .15*sin(angle*8-EffectAge*5+radius*8) + .09*sin(angle*17+EffectAge*7-radius*15);
    float ember = (.57+.43*sin(radius*31+angle*9-EffectAge*10)) * (.7+.3*cos(angle*15+EffectAge*3));
    float tongues = (1-smoothstep(curl-.12,curl+.12,radius)) * (.48+.49*ember);
    float bands = pow(saturate(.5+.5*sin(radius*29+angle*5-EffectAge*7)),9);
    alpha = max(edge*.91,mask*(.43+tongues*.45));
    color = lerp(soot,Ink.rgb*2.25,saturate(tongues*.9 + bands*.26));
    color = lerp(color,Tip.rgb*3.0,saturate(tongues*tongues*.82 + Flash*.38));
    color = lerp(color, Ink.rgb * 1.34, edge*.48);
}
else if (Style < 4.5)
{
    // Ice cracks after the spike erupts. The warning itself has a red border.
    float spoke = pow(saturate(.5+.5*cos(angle*6+.1*sin(radius*25))),31);
    float branch = 1-smoothstep(.012,.035,abs(radius - (.5+.11*cos(angle*12))));
    float rim = 1-smoothstep(.009,.03,abs(radius-.92));
    float cracks = max(spoke*(1-smoothstep(.79,.98,radius)),branch*.65);
    alpha = max(cracks*.81,max(rim*.6,(1-smoothstep(.26,.93,radius))*.36));
    color = lerp(Ink.rgb*.38,Tip.rgb*3.07,saturate(cracks + Flash*.26));
}
else if (Style < 6.5)
{
    // Six-petal lotus for healing and an interlocked diamond border for inversion.
    float border = 1-smoothstep(.017,.038,abs(radius-.90));
    float petals = 1-smoothstep(.014,.034,abs(radius-(.51+.19*cos(angle*6))));
    float diamond = abs(p.x)+abs(p.y);
    float square = 1-smoothstep(.014,.039,abs(diamond-.83));
    float marks = (1-smoothstep(.020,.043,abs(radius-.75))) * smoothstep(.30,.65,cos(angle*12));
    float motif = Style < 5.5 ? petals : square;
    float echo = 1-smoothstep(.009,.030,abs(radius-lerp(.33,.81,frac(EffectAge*.92))));
    float shine = max(border,max(motif,max(marks*.76,echo*.32)));
    alpha = max(shine*.88,(1-smoothstep(.48,.92,radius))*.13);
    color = lerp(Ink.rgb*.67,Tip.rgb*3.2,saturate(shine*(.78+.22*Progress)));
}
else if (Style < 7.5)
{
    // Ground strip of the laser. Local X follows the beam, local Y is the real width.
    float halfWidth = abs(p.y);
    float ends = 1-smoothstep(.96,1,abs(p.x));
    float rim = 1-smoothstep(.023,.065,abs(halfWidth-.922));
    float dark = 1-smoothstep(.074,.094,abs(halfWidth-.922));
    float centerLine = (1-smoothstep(.025,.075,halfWidth)) * (.52+.48*Progress);
    float chevrons = pow(saturate(.5+.5*cos(UV.x*113 - halfWidth*6 - EffectAge*9)),12) * (1-smoothstep(.54,.84,halfWidth));
    alpha = ends * max(dark*.98,max(centerLine,max(chevrons*.52,(1-smoothstep(.88,1,halfWidth))*.24)));
    color = lerp(lerp(soot,Ink.rgb*.95,saturate(centerLine+chevrons*.58)),Tip.rgb*4.1,saturate(rim+centerLine*.7));
}
else
{
    // Flowing wake for the two moving hazards. All brightness lies inside the tapered silhouette.
    float x = UV.x;
    float tail = saturate(x*.99+.01);
    float reach = .13+.82*pow(tail,.78);
    float blur = 1-smoothstep(reach*.57,reach,abs(p.y));
    float strands = pow(saturate(.5+.5*sin(UV.x*43 - EffectAge*13 + p.y*p.y*12)),10);
    float sparks = pow(saturate(.5+.5*sin(UV.x*79 + p.y*43 - EffectAge*19)),19) * (1-smoothstep(.17,.87,abs(p.y)));
    float edge = 1-smoothstep(.91,1,x);
    alpha = blur * pow(tail,.61) * edge * (.24 + strands*.55 + sparks*.19);
    color = lerp(Ink.rgb*.91,Tip.rgb*4.0,saturate(strands*.7+sparks*.72+Flash*.17));
}
return float4(color, saturate(alpha * Opacity));
