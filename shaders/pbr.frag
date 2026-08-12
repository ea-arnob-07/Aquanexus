in vec3 vWorldPos;
in vec3 vNormal;
in vec2 vUV;
in vec4 vShadowPos;
out vec4 FragColor;
uniform vec3 uBaseColor;
uniform float uRoughness;
uniform float uMetallic;
uniform vec3 uSunDir;
uniform vec3 uSunColor;
uniform vec3 uCameraPos;
uniform sampler2D uShadowMap;
uniform int uReceiveShadow;
uniform int uMaterialMode;
uniform float uTime;
uniform float uNightBlend;

float hash21(vec2 p){
    p=fract(p*vec2(123.34,456.21)); p+=dot(p,p+45.32); return fract(p.x*p.y);
}
float valueNoise2D(vec2 p){
    vec2 i=floor(p),f=fract(p); f=f*f*(3.0-2.0*f);
    float a=hash21(i),b=hash21(i+vec2(1,0)),c=hash21(i+vec2(0,1)),d=hash21(i+vec2(1,1));
    return mix(mix(a,b,f.x),mix(c,d,f.x),f.y);
}
float fbm2(vec2 p){
    float s=0.0,a=0.51; mat2 r=mat2(0.80,-0.60,0.60,0.80);
    for(int i=0;i<6;i++){ s+=a*valueNoise2D(p); p=r*p*2.03+19.17; a*=0.49; }
    return s;
}
float triFbm(vec3 p,vec3 n){
    vec3 w=pow(abs(n)+0.0001,vec3(4.0)); w/=w.x+w.y+w.z;
    return fbm2(p.yz)*w.x+fbm2(p.xz)*w.y+fbm2(p.xy)*w.z;
}
float triNoise(vec3 p,vec3 n){
    vec3 w=pow(abs(n)+0.0001,vec3(4.0)); w/=w.x+w.y+w.z;
    return valueNoise2D(p.yz)*w.x+valueNoise2D(p.xz)*w.y+valueNoise2D(p.xy)*w.z;
}
vec3 perturbNormal(vec3 N,float height,float strength){
    vec3 dpdx=dFdx(vWorldPos),dpdy=dFdy(vWorldPos);
    float dhdx=dFdx(height),dhdy=dFdy(height);
    vec3 r1=cross(dpdy,N),r2=cross(N,dpdx);
    float det=dot(dpdx,r1);
    vec3 grad=(dhdx*r1+dhdy*r2)/max(abs(det),0.00001);
    return normalize(N-grad*strength*sign(det));
}
float shadowPCF(vec4 sp,vec3 N,vec3 L){
    vec3 q=sp.xyz/max(sp.w,0.0001); q=q*0.5+0.5;
    if(q.z>1.0||q.x<0.0||q.x>1.0||q.y<0.0||q.y>1.0) return 0.0;
    float bias=max(0.00055*(1.0-dot(N,L)),0.00012);
    vec2 texel=1.0/vec2(textureSize(uShadowMap,0));
    float s=0.0,w=0.0;
    for(int x=-2;x<=2;x++) for(int y=-2;y<=2;y++){
        float ww=1.0/(1.0+float(x*x+y*y));
        float d=texture(uShadowMap,q.xy+vec2(x,y)*texel).r;
        s+=(q.z-bias>d?1.0:0.0)*ww; w+=ww;
    }
    return s/max(w,0.001);
}
float D_GGX(float NoH,float r){ float a=r*r,a2=a*a,d=NoH*NoH*(a2-1.0)+1.0; return a2/max(3.14159265*d*d,0.0001); }
float G1(float NoV,float r){ float k=(r+1.0); k=k*k/8.0; return NoV/max(NoV*(1.0-k)+k,0.0001); }
vec3 F_Schlick(float VoH,vec3 F0){ return F0+(1.0-F0)*pow(1.0-VoH,5.0); }

vec3 material(inout float rough,inout float metal,inout vec3 N){
    vec3 base=uBaseColor; vec2 xz=vWorldPos.xz;
    if(uMaterialMode==1){
        // Village ground: grass/soil mosaic, dry foot tracks and damp patches.
        float macro=fbm2(xz*0.045), mid=fbm2(xz*0.23+7.2), micro=valueNoise2D(xz*3.6);
        float pathA=1.0-smoothstep(0.42,1.55,abs(vWorldPos.x+0.18*vWorldPos.z+1.1));
        float pathB=1.0-smoothstep(0.38,1.00,abs(vWorldPos.x-24.1));
        float pathC=1.0-smoothstep(0.38,1.00,abs(vWorldPos.x+26.2));
        float path=max(pathA*0.34,max(pathB,pathC));
        vec3 grass=mix(vec3(0.055,0.125,0.028),vec3(0.17,0.285,0.055),mid);
        vec3 soil=mix(vec3(0.19,0.105,0.041),vec3(0.43,0.285,0.12),macro);
        float dry=smoothstep(0.55,0.82,macro+mid*0.18);
        base=mix(grass,soil,clamp(path+dry*0.22,0.0,0.82));
        base*=0.78+0.30*micro; rough=0.96;
        float h=fbm2(xz*2.1),hx=fbm2((xz+vec2(0.03,0))*2.1),hz=fbm2((xz+vec2(0,0.03))*2.1);
        N=normalize(N+vec3((h-hx)*1.6,0.0,(h-hz)*1.6));
    } else if(uMaterialMode==2){
        // Eroded pond bank: compacted earth, exposed stone and wet waterline staining.
        float macro=fbm2(xz*0.38+vWorldPos.y*0.21), grain=valueNoise2D(xz*5.2+vWorldPos.y*1.3);
        float rock=smoothstep(0.71,0.91,valueNoise2D(xz*1.92+vec2(vWorldPos.y*2.0)));
        vec3 dryClay=mix(vec3(0.205,0.118,0.047),vec3(0.49,0.315,0.125),macro);
        vec3 stone=vec3(0.29,0.27,0.22)*(0.70+0.45*grain);
        base=mix(dryClay,stone,rock*0.57);
        float wet=1.0-smoothstep(0.34,0.78,vWorldPos.y);
        base=mix(base,base*vec3(0.42,0.50,0.47),wet*0.52);
        float streak=pow(valueNoise2D(vec2(xz.x*0.65+vWorldPos.y*4.0,xz.y*0.65)),4.0);
        base*=1.0-streak*0.13; rough=mix(0.97,0.73,wet);
        float h=valueNoise2D(xz*5.8),hx=valueNoise2D((xz+vec2(0.025,0))*5.8),hz=valueNoise2D((xz+vec2(0,0.025))*5.8);
        N=normalize(N+vec3((h-hx)*1.75,0.0,(h-hz)*1.75));
    } else if(uMaterialMode==3){
        // Broad leaves and bush crowns: chlorophyll variation, faint veins and waxy microstructure.
        float n=triFbm(vWorldPos*1.62,N),fleck=triNoise(vWorldPos*9.2,N);
        float midVein=exp(-abs(vUV.x-0.5)*34.0)*(0.30+0.70*smoothstep(0.05,0.85,vUV.y));
        float sideVein=0.5+0.5*sin(vUV.y*72.0+abs(vUV.x-0.5)*19.0);
        base*=mix(0.66,1.24,n);
        base=mix(base,base*vec3(1.12,1.09,0.65),smoothstep(0.72,0.94,fleck)*0.20);
        base*=1.0+midVein*0.08+sideVein*0.025;
        rough=mix(0.88,0.70,smoothstep(0.30,0.88,n)); metal=0.0;
        N=perturbNormal(N,n*0.72+fleck*0.16+midVein*0.12,0.10);
    } else if(uMaterialMode==9){
        // Field grass: blade-to-blade value changes, dry tips and a restrained dewy highlight.
        float n=fbm2(xz*1.80+vWorldPos.y*0.76),fine=valueNoise2D(xz*13.0+vWorldPos.y*5.0);
        float tip=smoothstep(0.62,1.0,vUV.y);
        base*=mix(0.62,1.28,n);
        base=mix(base,base*vec3(1.34,1.12,0.48),tip*smoothstep(0.63,0.91,fine)*0.28);
        rough=mix(0.91,0.72,tip*0.32); metal=0.0;
    } else if(uMaterialMode==4){
        // Bamboo, timber and bark: long fibres, growth scars and recessed creases.
        float rings=0.5+0.5*sin(vWorldPos.y*15.0+triFbm(vWorldPos*1.8,N)*5.0);
        float grain=triNoise(vWorldPos*vec3(5.0,1.7,5.0),N);
        float groove=pow(abs(sin((vWorldPos.x+vWorldPos.z)*23.0+grain*4.0)),10.0);
        base*=0.58+0.38*rings+0.17*grain;
        base*=1.0-groove*0.12; rough=0.90;
        N=perturbNormal(N,grain*0.58+rings*0.28-groove*0.14,0.16);
    } else if(uMaterialMode==5){
        // Thatch, dry leaves and sail cloth: layered fibres with darker gaps between bundles.
        float n=triFbm(vWorldPos*3.0,N);
        float fiber=0.5+0.5*sin((vWorldPos.x+vWorldPos.z+vWorldPos.y*0.18)*38.0+triNoise(vWorldPos*7.0,N)*4.0);
        float gap=pow(1.0-fiber,7.0);
        base*=0.60+0.28*fiber+0.25*n;
        base*=1.0-gap*0.20; rough=0.985;
        N=perturbNormal(N,n*0.55+fiber*0.33,0.12);
    } else if(uMaterialMode==6){
        // Brushed, weather-exposed machinery with fine scratches and restrained edge corrosion.
        float n=triFbm(vWorldPos*3.8,N),scratch=0.5+0.5*sin(vWorldPos.y*92.0+vWorldPos.x*7.0+vWorldPos.z*5.0);
        float aged=smoothstep(0.42,0.82,metal);
        float rustMask=smoothstep(0.72,0.91,triFbm(vWorldPos*1.55+vec3(13.1),N))*aged;
        base*=0.76+0.25*n;
        base=mix(base,base*0.62,pow(scratch,18.0)*0.13);
        base=mix(base,vec3(0.34,0.095,0.025)*(0.72+0.30*n),rustMask*0.42);
        metal=mix(metal,0.06,rustMask); rough=clamp(rough+0.10*n+rustMask*0.25,0.12,0.86);
        N=perturbNormal(N,n*0.42+scratch*0.05,0.055);
    } else if(uMaterialMode==7){
        float n=triFbm(vWorldPos*3.0,N),pore=triNoise(vWorldPos*18.0,N);
        base*=0.69+0.44*n; rough=0.92;
        N=perturbNormal(N,n*0.55+pore*0.16,0.11);
    } else if(uMaterialMode==8){
        float n=fbm2(vWorldPos.xz*4.5+vWorldPos.y); base=mix(base*0.52,base*1.08,n); rough=0.91;
    } else if(uMaterialMode==10){
        // Fish skin: overlapping scale response, lateral line and a soft blue-green iridescent rim.
        float stripe=0.5+0.5*sin(vWorldPos.z*17.0+fbm2(vWorldPos.xy*6.0)*2.0);
        float scaleRows=sin(vUV.y*118.0+sin(vUV.x*83.0)*0.85);
        float scales=smoothstep(0.28,0.96,scaleRows*0.5+0.5);
        float lateral=exp(-abs(vUV.y-0.52)*58.0);
        vec3 viewDir=normalize(uCameraPos-vWorldPos);
        float irid=pow(1.0-abs(dot(N,viewDir)),2.4);
        base=mix(base*0.72,base*1.18,stripe*0.34);
        base*=0.91+0.10*scales+0.08*lateral;
        base=mix(base,base*vec3(0.72,1.13,1.25),irid*0.18); rough=mix(0.34,0.22,irid);
        N=perturbNormal(N,scales*0.13+lateral*0.06,0.045);
    } else if(uMaterialMode==11){
        // Dry tropical village ground, deliberately separate from the locked submerged pond bed mode.
        float macro=fbm2(xz*0.052),mid=fbm2(xz*0.31+8.4),micro=valueNoise2D(xz*2.4);
        float pathA=1.0-smoothstep(0.42,1.55,abs(vWorldPos.x+0.18*vWorldPos.z+1.1));
        float pathB=1.0-smoothstep(0.38,1.00,abs(vWorldPos.x-24.1));
        float pathC=1.0-smoothstep(0.38,1.00,abs(vWorldPos.x+26.2));
        float path=max(pathA*0.36,max(pathB,pathC));
        vec3 grass=mix(vec3(0.045,0.115,0.024),vec3(0.17,0.30,0.055),mid);
        vec3 soil=mix(vec3(0.17,0.085,0.028),vec3(0.46,0.31,0.12),macro);
        float bare=clamp(path+smoothstep(0.60,0.83,macro+mid*0.14)*0.24,0.0,0.86);
        base=mix(grass,soil,bare);
        base*=0.86+0.20*micro; rough=0.97;
        N=perturbNormal(N,mid*0.56+micro*0.10,0.14);
    } else if(uMaterialMode==12){
        // Hand-smoothed mud plaster with hairline cracks, repairs and rain streaking.
        float n=triFbm(vWorldPos*2.15,N),pore=triNoise(vWorldPos*15.0,N);
        float crack=pow(1.0-abs(sin((vWorldPos.x+vWorldPos.y*0.37+vWorldPos.z)*18.0+n*8.0)),20.0);
        float rain=pow(valueNoise2D(vec2((vWorldPos.x+vWorldPos.z)*2.4,vWorldPos.y*0.32)),4.0);
        base*=0.70+0.38*n;
        base*=1.0-crack*0.18-rain*0.08; rough=0.985;
        N=perturbNormal(N,n*0.62+pore*0.12-crack*0.15,0.13);
    } else if(uMaterialMode==13){
        // Concrete, compacted clods and stone footings: aggregate speckles and chipped pores.
        float n=triFbm(vWorldPos*3.8,N),grain=triNoise(vWorldPos*22.0,N);
        float aggregate=smoothstep(0.78,0.95,grain);
        base*=0.72+0.38*n;
        base=mix(base,base*vec3(0.58,0.62,0.60),aggregate*0.30); rough=0.975;
        N=perturbNormal(N,n*0.48+grain*0.20,0.16);
    } else if(uMaterialMode==14){
        // Human skin: very low-amplitude pore and warm/cool blood-flow variation.
        float pore=triNoise(vWorldPos*34.0,N),tone=triFbm(vWorldPos*2.4,N);
        base*=0.91+0.16*tone;
        base=mix(base,base*vec3(1.08,0.91,0.86),smoothstep(0.68,0.90,tone)*0.16);
        rough=0.67; N=perturbNormal(N,pore,0.020);
    } else if(uMaterialMode==15){
        // Short animal hair and hide mottling without a plastic shine.
        float coat=triFbm(vWorldPos*5.0,N),hair=0.5+0.5*sin((vWorldPos.y+vWorldPos.z*0.42)*96.0+coat*6.0);
        base*=0.70+0.48*coat;
        base*=0.96+0.06*hair; rough=0.91;
        N=perturbNormal(N,coat*0.55+hair*0.08,0.065);
    } else if(uMaterialMode==16){
        // Woven clothing: crossed warp/weft highlights with subtle faded dye variation.
        float warp=0.5+0.5*sin((vWorldPos.x+vWorldPos.z)*92.0);
        float weft=0.5+0.5*sin(vWorldPos.y*104.0);
        float fade=triFbm(vWorldPos*2.6,N);
        base*=0.78+0.30*fade+0.035*(warp+weft); rough=0.91;
        N=perturbNormal(N,warp*0.08+weft*0.08+fade*0.30,0.045);
    }
    return max(base,vec3(0.001));
}

void main(){
    float rough=clamp(uRoughness,0.045,1.0), metal=clamp(uMetallic,0.0,1.0);
    vec3 N=normalize(vNormal), base=material(rough,metal,N);
    vec3 V=normalize(uCameraPos-vWorldPos), L=normalize(-uSunDir), H=normalize(V+L);
    float NoL=max(dot(N,L),0.0), NoV=max(dot(N,V),0.001), NoH=max(dot(N,H),0.0), VoH=max(dot(V,H),0.0);
    vec3 F0=mix(vec3(0.035),base,metal), F=F_Schlick(VoH,F0);
    float D=D_GGX(NoH,rough), G=G1(NoL,rough)*G1(NoV,rough);
    vec3 spec=(D*G*F)/max(4.0*NoL*NoV,0.001);
    vec3 kd=(1.0-F)*(1.0-metal);
    float sh=(uReceiveShadow==1)?shadowPCF(vShadowPos,N,L):0.0;
    vec3 direct=(kd*base/3.14159265+spec)*uSunColor*NoL*(1.0-sh);

    float hemi=N.y*0.5+0.5;
    vec3 dayIrr=mix(vec3(0.075,0.085,0.078),vec3(0.23,0.34,0.47),pow(hemi,0.55));
    vec3 moonIrr=mix(vec3(0.006,0.010,0.018),vec3(0.045,0.085,0.175),pow(hemi,0.62));
    vec3 skyIrr=mix(dayIrr,moonIrr,uNightBlend);
    vec3 dayBounce=vec3(0.19,0.105,0.043)*max(-N.y,0.0)*0.26;
    vec3 moonBounce=vec3(0.012,0.018,0.030)*max(-N.y,0.0)*0.17;
    vec3 groundBounce=mix(dayBounce,moonBounce,uNightBlend);
    float cavity=0.78+0.22*valueNoise2D(vWorldPos.xz*0.78+vWorldPos.y*0.6);
    vec3 ambient=(base*skyIrr*0.50+base*groundBounce+F0*0.030)*cavity;
    if(uMaterialMode==3 || uMaterialMode==9){
        float back=max(dot(-N,L),0.0);
        vec3 transmission=mix(vec3(0.38,0.52,0.15),vec3(0.09,0.18,0.26),uNightBlend);
        ambient += base*transmission*back*0.22*(1.0-sh*0.4);
    }
    vec3 color=ambient+direct;
    if(uMaterialMode==17){
        // Screens, lanterns and status LEDs become practical light cues after dark.
        color+=base*mix(0.34,4.6,uNightBlend);
    }
    if(uMaterialMode==10){
        // A restrained moonlit sheen keeps the larger night-time fish population readable.
        color+=base*vec3(0.045,0.085,0.16)*uNightBlend;
    }
    float dist=length(uCameraPos-vWorldPos);
    float fog=smoothstep(60.0,138.0,dist);
    vec3 fogColor=mix(vec3(0.56,0.67,0.70),vec3(0.018,0.042,0.085),uNightBlend);
    color=mix(color,fogColor,fog*0.62);
    FragColor=vec4(color,1.0);
}
