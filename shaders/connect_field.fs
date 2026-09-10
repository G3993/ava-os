/*{
  "DESCRIPTION": "Fluid Cards & Wires — drifting shaped cards with iridescent liquid marble fills, connected by animated curved wire arcs with traveling pulses. Rich geometric micro-decorations inside each card. Surreal field of halos, squiggles, and blobs in the background. Audio reactive throughout.",
  "CREDIT": "ShaderClaw Combined",
  "CATEGORIES": ["Generator","Geometry","Audio Reactive"],
  "INPUTS": [
    {"NAME":"paperTint","LABEL":"Paper Tint","TYPE":"color","DEFAULT":[0.905,0.900,0.890,1.0],"GROUP":"Color"},
    {"NAME":"inkColor","LABEL":"Wire / Ink Color","TYPE":"color","DEFAULT":[0.10,0.09,0.08,1.0],"GROUP":"Color"},
    {"NAME":"accentColor","LABEL":"Accent Color","TYPE":"color","DEFAULT":[0.97,0.42,0.13,1.0],"GROUP":"Color"},
    {"NAME":"cardHue","LABEL":"Card Hue Offset","TYPE":"float","MIN":0.0,"MAX":6.2832,"DEFAULT":0.0,"GROUP":"Color"},
    {"NAME":"cardSaturation","LABEL":"Card Saturation","TYPE":"float","MIN":0.0,"MAX":1.0,"DEFAULT":0.85,"GROUP":"Color"},
    {"NAME":"gradientSpeed","LABEL":"Gradient Speed","TYPE":"float","MIN":0.0,"MAX":3.0,"DEFAULT":0.8,"GROUP":"Color"},
    {"NAME":"paletteShift","LABEL":"Palette Shift","TYPE":"float","MIN":0.0,"MAX":1.0,"DEFAULT":0.0,"GROUP":"Color"},
    {"NAME":"hueShift","LABEL":"Hue Shift","TYPE":"float","MIN":0.0,"MAX":1.0,"DEFAULT":0.0,"GROUP":"Color"},
    {"NAME":"colorBoost","LABEL":"Color Boost","TYPE":"float","MIN":0.0,"MAX":2.0,"DEFAULT":1.0,"GROUP":"Color"},
    {"NAME":"brightness","LABEL":"Brightness","TYPE":"float","MIN":0.2,"MAX":3.0,"DEFAULT":1.0,"GROUP":"Color"},
    {"NAME":"warpAmount","LABEL":"Marble Warp","TYPE":"float","MIN":0.0,"MAX":1.5,"DEFAULT":0.65,"GROUP":"Motion / Animation"},
    {"NAME":"driftAmount","LABEL":"Card Drift","TYPE":"float","MIN":0.0,"MAX":1.0,"DEFAULT":0.5,"GROUP":"Motion / Animation"},
    {"NAME":"speed","LABEL":"Flow Speed","TYPE":"float","MIN":0.0,"MAX":2.0,"DEFAULT":0.35,"GROUP":"Motion / Animation"},
    {"NAME":"curveAmount","LABEL":"Wire Curvature","TYPE":"float","MIN":0.0,"MAX":1.0,"DEFAULT":0.6,"GROUP":"Shape / Geometry"},
    {"NAME":"pulseRate","LABEL":"Pulse Rate","TYPE":"float","MIN":0.0,"MAX":2.0,"DEFAULT":0.8,"GROUP":"Motion / Animation"},
    {"NAME":"inkAmt","LABEL":"Ink Pooling","TYPE":"float","MIN":0.0,"MAX":1.0,"DEFAULT":0.45,"GROUP":"Color"},
    {"NAME":"outlineAmt","LABEL":"Outline","TYPE":"float","MIN":0.0,"MAX":1.0,"DEFAULT":0.9,"GROUP":"Color"},
    {"NAME":"shadowAmt","LABEL":"Drop Shadow","TYPE":"float","MIN":0.0,"MAX":1.0,"DEFAULT":0.5,"GROUP":"Color"},
    {"NAME":"grainAmt","LABEL":"Paper Grain","TYPE":"float","MIN":0.0,"MAX":1.0,"DEFAULT":0.25,"GROUP":"Color"},
    {"NAME":"fieldAmount","LABEL":"Surreal Field","TYPE":"float","MIN":0.0,"MAX":1.5,"DEFAULT":0.8,"GROUP":"Shape / Geometry"},
    {"NAME":"audioReact","LABEL":"Audio Reactivity","TYPE":"float","MIN":0.0,"MAX":1.0,"DEFAULT":0.6,"GROUP":"Audio Reactivity"}
  ]
}*/

// ── Math helpers ─────────────────────────────────────────────────────────────
#define PI 3.14159265

float hash11(float p){
    p=fract(p*0.1031); p*=p+33.33; p*=p+p; return fract(p);
}
float hash21(vec2 p){
    return fract(sin(dot(p,vec2(127.1,311.7)))*43758.5453);
}
float hash21b(vec2 p){
    return fract(sin(dot(p,vec2(41.3,289.1)))*43758.5453);
}
vec2 hash22(vec2 p){
    return fract(sin(vec2(dot(p,vec2(127.1,311.7)),dot(p,vec2(269.5,183.3))))*43758.5453);
}
float h12(vec2 p){return fract(sin(dot(p,vec2(127.1,311.7)))*43758.5453);}

float vnoise(vec2 p){
    vec2 i=floor(p),f=fract(p);
    f=f*f*(3.0-2.0*f);
    float a=h12(i),b=h12(i+vec2(1.0,0.0)),c=h12(i+vec2(0.0,1.0)),d=h12(i+vec2(1.0,1.0));
    return mix(mix(a,b,f.x),mix(c,d,f.x),f.y);
}
float fbm(vec2 p){
    float v=0.0,a=0.5;
    for(int i=0;i<4;i++){v+=a*vnoise(p);p=p*2.02+vec2(11.3,7.7);a*=0.5;}
    return v;
}

vec3 hsl2rgb(float h,float s,float l){
    vec3 rgb=clamp(abs(mod(h*6.0+vec3(0.0,4.0,2.0),6.0)-3.0)-1.0,0.0,1.0);
    return l+s*(rgb-0.5)*(1.0-abs(2.0*l-1.0));
}

// Eight-stop iridescent palette
vec3 PAL(float t){
    vec3 S[8];
    S[0]=vec3(0.98,0.42,0.66);
    S[1]=vec3(0.99,0.62,0.36);
    S[2]=vec3(0.99,0.84,0.42);
    S[3]=vec3(0.36,0.82,0.66);
    S[4]=vec3(0.20,0.66,0.86);
    S[5]=vec3(0.34,0.40,0.92);
    S[6]=vec3(0.62,0.42,0.95);
    S[7]=vec3(0.93,0.50,0.84);
    float x=fract(t)*8.0;
    int ii=int(x);
    float f=x-float(ii);
    f=f*f*f*(f*(f*6.0-15.0)+10.0);
    int jj=ii+1; if(jj>7) jj=0;
    vec3 Sa=S[0],Sb=S[0];
    for(int k=0;k<8;k++){if(k==ii)Sa=S[k];if(k==jj)Sb=S[k];}
    vec3 col=mix(Sa,Sb,f);
    vec3 irid=0.5+0.5*cos(6.2831853*(t*vec3(1.0,1.07,1.13)+vec3(0.0,0.33,0.67)));
    col=mix(col,col*(0.78+0.5*irid),0.32);
    return clamp(col,0.0,1.0);
}

// ── SDFs ────────────────────────────────────────────────────────────────────
float sdRRect(vec2 d,vec2 he,float r){
    vec2 q=abs(d)-he+r;
    return length(max(q,0.0))+min(max(q.x,q.y),0.0)-r;
}
float sdEllipse(vec2 p,vec2 ab){
    vec2 q=abs(p)/ab;
    float l=length(q);
    return (l-1.0)*min(ab.x,ab.y);
}
float sdDiamond(vec2 p,vec2 he){
    vec2 q=abs(p);
    return (q.x*he.y+q.y*he.x-he.x*he.y)/length(he);
}
float sdHex(vec2 p,float r){
    const vec3 k=vec3(-0.866025,0.5,0.57735);
    vec2 q=abs(p);
    q-=2.0*min(dot(k.xy,q),0.0)*k.xy;
    q-=vec2(clamp(q.x,-k.z*r,k.z*r),r);
    return length(q)*sign(q.y);
}
float sdPill(vec2 p,vec2 he){
    float h=max(he.y-he.x,0.0);
    vec2 q=vec2(abs(p.x),abs(p.y)-h);
    return length(max(q,0.0))+min(max(q.x,q.y),0.0)-he.x;
}
float sdShield(vec2 p,vec2 he){
    float top=sdRRect(p-vec2(0.0,he.y*0.15),he*vec2(1.0,0.85),he.x*0.35);
    vec2 tp=p+vec2(0.0,he.y*0.35);
    float tri=sdDiamond(tp,vec2(he.x,he.y*0.65));
    return min(top,tri);
}
float sdSquircle(vec2 p,vec2 he){
    vec2 q=abs(p)/he;
    float v=pow(q.x,4.0)+pow(q.y,4.0);
    return (pow(v,0.25)-1.0)*min(he.x,he.y);
}

float cardSDF(vec2 d,vec2 he,float r,float st){
    if(st<0.5) return sdRRect(d,he,r);
    if(st<1.5) return sdEllipse(d,he);
    if(st<2.5) return sdDiamond(d,he*1.1);
    if(st<3.5) return sdHex(d,min(he.x,he.y));
    if(st<4.5) return sdPill(d,he);
    if(st<5.5) return sdShield(d,he);
    return sdSquircle(d,he);
}

// ── Global state ─────────────────────────────────────────────────────────────
float gCk,gAR,gBassP,gMidP;

// ── Card layout ──────────────────────────────────────────────────────────────
float cardShapeType(float id){
    if(id<0.5) return 0.0;
    if(id<1.5) return 1.0;
    if(id<2.5) return 2.0;
    if(id<3.5) return 3.0;
    if(id<4.5) return 4.0;
    if(id<5.5) return 5.0;
    return 6.0;
}
vec2 cardBase(float id){
    if(id<0.5) return vec2(-0.560, 0.300);
    if(id<1.5) return vec2(-0.045, 0.410);
    if(id<2.5) return vec2( 0.500, 0.330);
    if(id<3.5) return vec2( 0.030,-0.030);
    if(id<4.5) return vec2(-0.530,-0.290);
    if(id<5.5) return vec2( 0.545,-0.190);
    return              vec2( 0.090,-0.450);
}
vec2 cardHalf(float id){
    if(id<0.5) return vec2(0.110,0.160);
    if(id<1.5) return vec2(0.100,0.140);
    if(id<2.5) return vec2(0.105,0.165);
    if(id<3.5) return vec2(0.115,0.150);
    if(id<4.5) return vec2(0.090,0.130);
    if(id<5.5) return vec2(0.100,0.155);
    return              vec2(0.112,0.135);
}
vec2 cardPos(float id){
    vec2 b=cardBase(id);
    float w=driftAmount*0.062;
    return b+w*vec2(sin(gCk*0.34+id*2.33),cos(gCk*0.27+id*1.71));
}
float cardScale(){
    return 1.0+0.008*sin(gCk*0.31)+0.05*gAR*gBassP;
}

// ── Wire connections ─────────────────────────────────────────────────────────
vec2 edgeIds(float e){
    if(e<0.5)  return vec2(0.0,1.0);
    if(e<1.5)  return vec2(1.0,2.0);
    if(e<2.5)  return vec2(3.0,0.0);
    if(e<3.5)  return vec2(1.0,3.0);
    if(e<4.5)  return vec2(2.0,5.0);
    if(e<5.5)  return vec2(3.0,5.0);
    if(e<6.5)  return vec2(3.0,4.0);
    if(e<7.5)  return vec2(4.0,6.0);
    if(e<8.5)  return vec2(3.0,6.0);
    if(e<9.5)  return vec2(0.0,4.0);
    return             vec2(5.0,6.0);
}
float segDist2(vec2 q2,vec2 a,vec2 b,out float h){
    vec2 pa=q2-a,ba=b-a;
    h=clamp(dot(pa,ba)/max(dot(ba,ba),1e-6),0.0,1.0);
    return length(pa-ba*h);
}

// ── Liquid marble fill ────────────────────────────────────────────────────────
vec3 liquidMarble(vec2 luv,float seed,float t,float warp,float pshift,float inkA){
    vec2 p=luv*vec2(2.4,3.0)+seed*7.13;
    for(int k=0;k<4;k++){
        float fk=float(k);
        vec2 flow=vec2(fbm(p*1.30+t*(0.22+0.05*fk)+seed+fk),
                       fbm(p.yx*1.30-t*(0.18+0.04*fk)+seed+4.0+fk));
        flow-=0.5;
        p+=warp*(0.65+0.35*fk*0.5)*flow;
        p+=0.06*vec2(sin(p.y*2.0+t*0.6),cos(p.x*2.0-t*0.5));
    }
    float n=fbm(p+t*0.07);
    float m=fbm(p*1.9-n*1.6+seed);
    float s=fbm(p*3.3+m*2.0-t*0.12);
    float hh=n*1.30+m*0.55+s*0.18+pshift+luv.y*0.22+seed*0.07;
    vec3 col=PAL(hh+cardHue/6.2832);
    col=mix(col,PAL(hh*1.7+0.35),smoothstep(0.45,0.85,s)*0.35);
    float sheen=smoothstep(0.60,0.95,m)*(0.55+0.35*s);
    col=mix(col,vec3(1.0,0.99,0.97),sheen*0.70);
    col+=vec3(0.30,0.22,0.40)*pow(sheen,3.0)*0.5;
    col=mix(col,vec3(0.025,0.03,0.05),smoothstep(0.26,0.03,m)*inkA);
    return clamp(col,0.0,1.2);
}

// ── Micro-decorations inside cards ──────────────────────────────────────────
float cardDecoration(float id,vec2 lp,vec2 he,float px){
    vec2 ln=lp/he;
    float acc=0.0;
    float t=gCk;
    float dType=mod(id,7.0);

    // Central ring on every card
    float cr=0.32+0.06*sin(t*0.7+id);
    float ring=length(ln)-cr;
    acc=max(acc,smoothstep(px*2.0,px*0.5,abs(ring*min(he.x,he.y))-px*1.2));

    if(dType<1.0){
        // Three circles in a row
        for(int k=0;k<3;k++){
            vec2 cp=vec2(-0.55+float(k)*0.55,0.0);
            acc=max(acc,smoothstep(px*2.5,0.0,length(ln-cp)-0.12));
        }
        acc=max(acc,smoothstep(px*1.5,0.0,abs(ln.y-0.62)*min(he.x,he.y)-px)*step(abs(ln.x),0.5));
    } else if(dType<2.0){
        // Plus cross + orbiting dot
        acc=max(acc,smoothstep(px*1.5,0.0,min(abs(ln.x),abs(ln.y))*min(he.x,he.y)-px*1.0)*step(max(abs(ln.x),abs(ln.y)),0.55));
        float a=t*0.9+id;
        acc=max(acc,smoothstep(px*2.5,0.0,length(ln-vec2(cos(a),sin(a))*0.55)-0.10));
    } else if(dType<3.0){
        // Diamond outline + center dot
        vec2 dq=abs(ln);
        float dfSDF=(dq.x+dq.y-0.6)*min(he.x,he.y)*0.7;
        acc=max(acc,smoothstep(px*2.0,0.0,abs(dfSDF)-px*1.2));
        acc=max(acc,smoothstep(px*2.0,0.0,length(ln)-0.10));
    } else if(dType<4.0){
        // Radial tick marks (hub card)
        for(int k=0;k<6;k++){
            float ang=float(k)*1.0472+t*0.3;
            vec2 rv=vec2(cos(ang),sin(ang));
            float proj2=dot(ln,rv);
            float perp=length(ln-rv*proj2)*min(he.x,he.y);
            acc=max(acc,smoothstep(px*1.5,0.0,perp-px)*step(0.38,proj2)*step(proj2,0.62));
        }
        float pr=0.22+0.07*sin(t*2.1);
        acc=max(acc,smoothstep(px*2.0,0.0,abs(length(ln)-pr)*min(he.x,he.y)-px*1.0));
    } else if(dType<5.0){
        // Inner pill + two poles
        float ps=sdPill(lp,he*vec2(0.35,0.55))/min(he.x,he.y);
        acc=max(acc,smoothstep(px*2.0,0.0,abs(ps*min(he.x,he.y))-px*1.0));
        acc=max(acc,smoothstep(px*2.5,0.0,length(ln-vec2(0.0, 0.62))-0.09));
        acc=max(acc,smoothstep(px*2.5,0.0,length(ln-vec2(0.0,-0.62))-0.09));
    } else if(dType<6.0){
        // Triangle + rotating dot
        vec2 tp=ln;
        float tri=max(abs(tp.x)*1.732+tp.y,-tp.y*2.0)-0.5;
        acc=max(acc,smoothstep(px*2.0,0.0,abs(tri*min(he.x,he.y)*0.7)-px*1.0));
        float a2=t*1.1+id*0.9;
        acc=max(acc,smoothstep(px*2.5,0.0,length(ln-vec2(cos(a2),sin(a2))*0.35)-0.09));
    } else {
        // Concentric rings + corner dots
        float r1=length(ln);
        acc=max(acc,smoothstep(px*2.0,0.0,abs(r1-0.28)*min(he.x,he.y)-px*1.0));
        acc=max(acc,smoothstep(px*2.0,0.0,abs(r1-0.55)*min(he.x,he.y)-px*1.0));
        for(int k=0;k<4;k++){
            float ang=float(k)*1.5708+0.7854;
            acc=max(acc,smoothstep(px*2.0,0.0,length(ln-vec2(cos(ang),sin(ang))*0.72)-0.09));
        }
    }

    // Wandering dot on every card
    float wa=t*0.55+id*2.3;
    vec2 wp=vec2(cos(wa),sin(wa*1.3))*0.4;
    acc=max(acc,smoothstep(px*2.5,0.0,length(ln-wp)-0.08));

    // Crosshair lines (subtle)
    acc=max(acc,smoothstep(px*1.2,0.0,abs(ln.y)*min(he.x,he.y)-px*0.6)*step(abs(ln.x),0.75)*0.4);
    acc=max(acc,smoothstep(px*1.2,0.0,abs(ln.x)*min(he.x,he.y)-px*0.6)*step(abs(ln.y),0.75)*0.4);

    return clamp(acc,0.0,1.0);
}

// ── Surreal background field ──────────────────────────────────────────────────
float segDistF(vec2 p,vec2 a,vec2 b){
    vec2 pa=p-a,ba=b-a;
    float h=clamp(dot(pa,ba)/dot(ba,ba),0.0,1.0);
    return length(pa-ba*h);
}

void applyField(inout vec3 col,vec2 p,float aspect,float t,float au){
    float FA=fieldAmount;
    if(FA<=0.001) return;

    // Sun
    {
        vec2 c=vec2(aspect*0.46,0.80)+0.012*vec2(sin(t*0.3),cos(t*0.2));
        float d=length(p-c);
        vec3 sun=mix(vec3(0.99,0.86,0.26),vec3(0.98,0.52,0.55),smoothstep(0.0,0.20,d));
        col+=sun*smoothstep(0.20,0.0,d)*(0.55+0.30*au)*FA;
        col+=vec3(0.99,0.90,0.55)*exp(-d*7.0)*0.40*FA;
    }

    // Drifting halos
    for(int i=0;i<5;i++){
        float fi=float(i);
        vec2 c=vec2(hash21(vec2(fi,1.0))*aspect,hash21(vec2(fi,2.0)));
        c+=0.018*vec2(sin(t*0.4+fi),cos(t*0.5+fi));
        float d=length(p-c);
        float r=mix(0.06,0.17,hash21(vec2(fi,3.0)));
        col+=PAL(hash21(vec2(fi,4.0))+paletteShift)*exp(-d*d/(r*r))*0.28*FA;
    }

    // Gradient puddles
    for(int i=0;i<2;i++){
        vec2 c=(i==0)?vec2(aspect*0.52,0.15):vec2(aspect*0.74,0.56);
        vec2 rad=(i==0)?vec2(0.32,0.10):vec2(0.21,0.085);
        vec2 fp2=p+0.10*vec2(fbm(p*4.0+t*0.25),fbm(p.yx*4.0-t*0.22));
        float d=length((p-c)/rad);
        float m=smoothstep(1.0,0.55,d)*0.75*FA;
        float mb=fbm(fp2*5.0+(i==0?t*0.18:-t*0.16));
        vec3 pc=(i==0)?PAL(0.55+mb*0.45+paletteShift):PAL(0.18+mb*0.40+paletteShift);
        pc=mix(pc,vec3(1.0),smoothstep(0.72,0.95,mb)*0.35);
        col=mix(col,pc,m);
    }

    // Ink blobs
    for(int i=0;i<6;i++){
        float fi=float(i);
        vec2 c=vec2(hash21(vec2(fi,11.0))*aspect,hash21(vec2(fi,12.0)));
        float ang=hash21(vec2(fi,13.0))*6.2831;
        float ca=cos(ang),sa=sin(ang);
        mat2 rot=mat2(ca,-sa,sa,ca);
        vec2 q2=rot*(p-c);
        vec2 scl=vec2(1.0,mix(0.5,2.4,hash21(vec2(fi,14.0))));
        float rb=mix(0.018,0.055,hash21(vec2(fi,15.0)));
        float warp2=0.20*fbm(q2*9.0+fi);
        float d=length(q2/scl)-rb*(1.0+warp2);
        float fill=smoothstep(0.004,-0.004,d)*0.7*FA;
        vec3 ik=vec3(0.04,0.045,0.06);
        vec2 hp=c+mat2(ca,sa,-sa,ca)*vec2(-rb*0.35,rb*0.5*scl.y);
        ik+=vec3(0.92)*smoothstep(rb*0.5,0.0,length(p-hp))*0.6;
        col=mix(col,ik,fill);
    }

    // Speck swarm
    for(int L=0;L<2;L++){
        float cell=(L==0)?0.045:0.085;
        vec2 g=p/cell,id=floor(g),f=fract(g)-0.5;
        vec2 rnd=hash22(id+float(L)*7.0);
        float on=step(0.42,hash21(id+float(L)*3.7+0.5));
        float d=length(f-(rnd-0.5)*0.6)*cell;
        float sz=mix(0.0025,0.0085,hash21(id+1.7));
        float tw=clamp(0.55+0.45*sin(t*3.5+hash21(id)*40.0)*au,0.0,1.0);
        float m=smoothstep(sz,sz*0.35,d)*on*0.65*FA*tw;
        vec3 spc=(hash21(id+9.1)<0.22)?vec3(0.05,0.05,0.07):PAL(hash21(id+0.3)+paletteShift);
        col=mix(col,spc,m);
    }

    // Wavy squiggles
    for(int k=0;k<4;k++){
        float fk=float(k);
        float cy=0.15+0.22*fk+0.02*sin(t*0.3+fk);
        float yl=cy+0.035*sin(p.x*(7.0+fk*3.0)+t*0.8+fk*2.0)+0.018*fbm(p*4.0+fk*5.0);
        float win=smoothstep(0.0,0.12,p.x)*smoothstep(aspect,aspect-0.12,p.x);
        float m=smoothstep(0.004,0.0015,abs(p.y-yl))*win*0.85*FA;
        col=mix(col,PAL(0.08+0.25*fk+t*0.02+paletteShift),m);
    }

    // Lollipop antennas
    for(int i=0;i<5;i++){
        float fi=float(i);
        float x=mix(0.06,aspect-0.06,hash21(vec2(fi,21.0)));
        float y0=0.10+0.5*hash21(vec2(fi,22.0));
        float y1=y0+mix(0.10,0.26,hash21(vec2(fi,23.0)));
        float ds=segDistF(p,vec2(x,y0),vec2(x,y1));
        col=mix(col,vec3(0.10,0.10,0.12),smoothstep(0.0018,0.0006,ds)*FA);
        col=mix(col,PAL(hash21(vec2(fi,24.0))+paletteShift),
                smoothstep(0.012,0.006,length(p-vec2(x,y1)))*FA);
    }
}

void main(){
    vec2 uv=isf_FragNormCoord.xy;
    float aspect=RENDERSIZE.x/RENDERSIZE.y;
    // Centered coords for card system: x in [-aspect*0.71, +aspect*0.71]
    vec2 q=(uv-0.5)*vec2(aspect,1.0)*1.42;

    gAR   =clamp(audioReact,0.0,1.0);
    gBassP=pow(clamp(audioBass/0.85,0.0,1.0),1.6);
    gMidP =pow(clamp(audioMid/0.85,0.0,1.0),1.3);
    float levP=clamp(audioLevel/0.85,0.0,1.0);
    float t=TIME*(speed+audioBass*gAR*0.30)+audioLevel*gAR*0.4;
    gCk=t;

    vec3 inkCol=inkColor.rgb;

    // ── Paper background ──────────────────────────────────────────────────────
    vec3 col=paperTint.rgb;
    col*=1.13-0.33*dot(q,q);
    float gn=vnoise(gl_FragCoord.xy*1.7)-0.5;
    col+=gn*grainAmt*0.05;
    // texture tooth
    vec2 hp2=uv*RENDERSIZE.xy*1.15;
    float tooth=0.5+0.5*sin(hp2.x)*sin(hp2.y);
    col*=1.0-0.03*tooth;

    // ── Surreal field (behind everything) ────────────────────────────────────
    vec2 fp=gl_FragCoord.xy/RENDERSIZE.y;
    applyField(col,fp,aspect,t,clamp(audioLevel*gAR,0.0,1.0));

    float px=1.6/RENDERSIZE.y;
    float scl=cardScale();

    // ── Drop shadows for all cards ───────────────────────────────────────────
    {
        vec2 shOff=vec2(0.018,-0.022);
        for(int i=0;i<7;i++){
            float fi=float(i);
            vec2 c=cardPos(fi)+shOff;
            vec2 he=cardHalf(fi)*scl;
            float r=min(he.x,he.y)*0.22;
            float st=cardShapeType(fi);
            float sd=cardSDF(q-c,he,r,st);
            float aSh=smoothstep(0.042,0.0,sd);
            col=mix(col,col*0.65,aSh*shadowAmt);
        }
    }

    // ── Wires / Arcs + pulses ─────────────────────────────────────────────────
    float arcA=0.0,glow=0.0,dotA=0.0;

    for(int e=0;e<11;e++){
        float fe=float(e);
        vec2 ids=edgeIds(fe);
        vec2 A=cardPos(ids.x);
        vec2 B=cardPos(ids.y);
        vec2 dir=normalize(B-A+vec2(1e-5));
        vec2 heA=cardHalf(ids.x)*scl;
        vec2 heB=cardHalf(ids.y)*scl;
        vec2 P0=A+dir*(min(heA.x,heA.y)*0.6);
        vec2 P2=B-dir*(min(heB.x,heB.y)*0.6);
        vec2 perp=vec2(-dir.y,dir.x);
        float bow=(hash11(fe*5.7+2.2)-0.5)*0.85*curveAmount
                  +0.055*sin(gCk*0.38+fe*1.9)
                  +0.11*gAR*gMidP*sin(gCk*0.8+fe*2.13);
        vec2 P1=(P0+P2)*0.5+perp*bow;

        float minD=1e9,tC=0.0;
        vec2 prev=P0;
        for(int s=1;s<=14;s++){
            float tt=float(s)/14.0,mt=1.0-tt;
            vec2 pt=mt*mt*P0+2.0*mt*tt*P1+tt*tt*P2;
            float hh;
            float d=segDist2(q,prev,pt,hh);
            if(d<minD){minD=d;tC=(float(s)-1.0+hh)/14.0;}
            prev=pt;
        }

        // Wire line (two thicknesses: main + thin highlight)
        float wireW=px*1.8+px*1.2*gAR*gMidP;
        arcA=max(arcA,smoothstep(wireW*1.5,wireW*0.4,minD));

        // Traveling pulses
        float band=exp(-minD*minD/0.0008);
        float pp=fract(gCk*0.075*(0.6+pulseRate)+hash11(fe*3.3)*7.0);
        glow+=exp(-pow((tC-pp)*6.5,2.0))*band*(0.55+0.40*levP*gAR);
        float pq=fract(-gCk*0.058*(0.6+pulseRate)+hash11(fe*5.1)*5.0);
        glow+=exp(-pow((tC-pq)*7.5,2.0))*band*(0.32+0.26*levP*gAR);

        // Hub beat pulse from card 3
        float fromHub=0.0,tHub=tC;
        if(ids.x==3.0) fromHub=1.0;
        if(ids.y==3.0){fromHub=1.0;tHub=1.0-tC;}
        float bp=clamp(audioBass*gAR,0.0,1.0);
        if(fromHub>0.5)
            glow+=exp(-pow((tHub-(1.0-pow(bp,0.8)))*5.5,2.0))*band*bp*gAR*1.8;

        // Terminal dots at wire ends
        dotA=max(dotA,smoothstep(px*4.0,px*2.0,length(q-P0)));
        dotA=max(dotA,smoothstep(px*4.0,px*2.0,length(q-P2)));
    }

    // Apply wires
    col=mix(col,inkCol,arcA*0.90);
    vec3 pulseCol=mix(inkCol,vec3(1.0,0.97,0.88),0.78);
    col+=pulseCol*glow*0.80;

    // ── Cards ─────────────────────────────────────────────────────────────────
    float warp=warpAmount+gMidP*gAR*0.40;
    float pshift=paletteShift+audioHigh*gAR*0.15;

    for(int i=0;i<7;i++){
        float fi=float(i);
        vec2 c=cardPos(fi);
        vec2 he=cardHalf(fi)*scl;
        vec2 d=q-c;
        float r=min(he.x,he.y)*0.22;
        float st=cardShapeType(fi);

        float sd=cardSDF(d,he,r,st);
        float aa=px*1.2;
        float aC=smoothstep(aa,-aa*0.5,sd);
        if(aC<=0.0) continue;

        // Map local position to 0..1 for marble
        vec2 luv=(d+he)/(2.0*he);
        luv=clamp(luv,0.0,1.0);

        // Liquid marble fill
        float seed=fi*3.71+1.0;
        vec3 marbleCol=liquidMarble(luv,seed,t,warp,pshift,inkAmt);

        // Blend marble with card saturation
        float luma=dot(marbleCol,vec3(0.299,0.587,0.114));
        marbleCol=mix(vec3(luma),marbleCol,cardSaturation);

        // Subtle top-light
        vec2 ln=d/he;
        marbleCol*=1.0+0.05*clamp(ln.y,-1.0,1.0);

        col=mix(col,marbleCol,aC);

        // Hairline border
        float border=smoothstep(px*1.8,px*0.4,abs(sd+px*0.8));
        col=mix(col,inkCol,border*outlineAmt*0.85*aC);

        // Micro geometric decorations
        float decal=cardDecoration(fi,d,he,px);
        col=mix(col,inkCol,decal*0.50*aC);

        // Hub beat flash
        float bp2=clamp(audioBass*gAR,0.0,1.0);
        if(i==3){
            col=mix(col,marbleCol*1.15+pulseCol*0.12,aC*bp2*gAR*0.5);
        }
    }

    // Terminal dots on top
    col=mix(col,inkCol,dotA*0.92);

    // ── Color grading ─────────────────────────────────────────────────────────
    // Color boost / saturation
    float ucL=dot(col,vec3(0.299,0.587,0.114));
    col=mix(vec3(ucL),col,colorBoost);

    // Hue shift
    if(hueShift>0.0005){
        float hA=hueShift*6.2831853;
        float hC=cos(hA),hS=sin(hA);
        mat3 hM=mat3(0.299,0.587,0.114,0.299,0.587,0.114,0.299,0.587,0.114)
               +hC*mat3(0.701,-0.587,-0.114,-0.299,0.413,-0.114,-0.300,-0.588,0.886)
               +hS*mat3(0.168,0.330,-0.497,-0.328,0.035,0.292,1.250,-1.050,-0.203);
        col=clamp(hM*col,0.0,1.0);
    }

    // Final grain + brightness
    float grain2=hash21b(uv*RENDERSIZE.xy);
    col+=(grain2-0.5)*grainAmt*0.06;

    float lift=mix(1.0,0.88+0.20*levP,gAR);
    col*=brightness*lift;

    gl_FragColor=vec4(clamp(col,0.0,1.0),1.0);
}