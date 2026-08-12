in vec2 vUV;
out vec4 FragColor;
uniform mat4 uInvViewProj;
uniform vec3 uCameraPos;
uniform vec3 uSunDir;
uniform float uTime;
uniform float uNightBlend;
float h2(vec2 p){ return fract(sin(dot(p,vec2(127.1,311.7)))*43758.5453123); }
float n2(vec2 p){ vec2 i=floor(p),f=fract(p); f=f*f*(3.0-2.0*f); return mix(mix(h2(i),h2(i+vec2(1,0)),f.x),mix(h2(i+vec2(0,1)),h2(i+vec2(1,1)),f.x),f.y); }
float fb(vec2 p){ float s=0.0,a=0.52; mat2 r=mat2(.8,-.6,.6,.8); for(int i=0;i<6;i++){s+=a*n2(p);p=r*p*2.03+11.7;a*=0.49;}return s; }
float ridged(vec2 p){
    float s=0.0,a=0.54;
    for(int i=0;i<5;i++){
        float n=1.0-abs(n2(p)*2.0-1.0);
        s+=n*n*a; p=mat2(.78,-.62,.62,.78)*p*2.12+7.3; a*=0.47;
    }
    return s;
}
void main(){
    vec2 ndc=vUV*2.0-1.0; vec4 fp=uInvViewProj*vec4(ndc,1,1); vec3 ray=normalize(fp.xyz/fp.w-uCameraPos);
    float y=clamp(ray.y,0.0,1.0);
    vec3 horizon=vec3(0.55,0.71,0.81),zenith=vec3(0.050,0.185,0.43);
    vec3 c=mix(horizon,zenith,pow(y,0.52));
    // Dense humid atmosphere at the horizon, with a faint warm lobe around the sun direction.
    vec3 sunD=normalize(-uSunDir); float sun=max(dot(ray,sunD),0.0);
    float warmHorizon=pow(sun,7.0)*exp(-max(ray.y,0.0)*5.5);
    c=mix(vec3(0.47,0.61,0.64),c,smoothstep(-0.12,0.12,ray.y));
    c=mix(c,vec3(0.88,0.67,0.43),warmHorizon*0.14);
    float horizonHaze=exp(-max(ray.y,0.0)*13.0);
    c=mix(c,vec3(0.62,0.73,0.76),horizonHaze*0.24);
    c+=vec3(1.0,0.86,0.66)*pow(sun,1500.0)*10.0;
    c+=vec3(1.0,0.73,0.48)*pow(sun,35.0)*0.16;
    if(ray.y>0.035){
        vec2 cp=ray.xz/max(ray.y,0.055)*0.20+vec2(uTime*0.0012,-uTime*0.0005);
        float c1=fb(cp*2.0),c2=fb(cp*4.7+9.3),erosion=ridged(cp*7.1+18.0);
        float body=c1*0.72+c2*0.20+erosion*0.08;
        float cloud=smoothstep(0.565,0.79,body)*smoothstep(0.02,0.16,ray.y);
        float rim=smoothstep(0.50,0.67,body)-smoothstep(0.68,0.80,body);
        float lit=pow(max(dot(ray,sunD),0.0),4.0);
        float underside=smoothstep(0.57,0.79,c1)*(1.0-smoothstep(0.22,0.55,ray.y));
        vec3 cloudShadow=mix(vec3(0.50,0.58,0.62),vec3(0.68,0.72,0.72),erosion);
        vec3 cloudLight=mix(vec3(0.84,0.87,0.86),vec3(1.08,1.01,0.90),0.30+0.70*lit);
        vec3 cloudCol=mix(cloudLight,cloudShadow,underside*0.50);
        cloudCol+=vec3(1.0,0.86,0.63)*rim*pow(sun,12.0)*0.28;
        c=mix(c,cloudCol,cloud*0.63);

        // Thin high-altitude cirrus has its own direction and slower drift.
        vec2 wispyUV=ray.xz/max(ray.y,0.09)*0.115+vec2(-uTime*0.00035,uTime*0.00022);
        float streak=fb(vec2(wispyUV.x*8.6+wispyUV.y*1.2,wispyUV.y*2.8)+31.0);
        float wispy=smoothstep(0.61,0.82,streak)*smoothstep(0.10,0.34,ray.y);
        c=mix(c,vec3(0.84,0.89,0.92),wispy*0.20);
    }
    // Gentle aerial desaturation near the geometric horizon integrates distant hills and sky.
    float aerial=exp(-abs(ray.y)*24.0);
    c=mix(c,vec3(dot(c,vec3(0.299,0.587,0.114))),aerial*0.075);

    // Default-on night mood: deep humid sky, stable stars, moon halo and moonlit clouds.
    vec3 nightHorizon=vec3(0.030,0.067,0.125),nightZenith=vec3(0.0035,0.009,0.030);
    vec3 night=mix(nightHorizon,nightZenith,pow(y,0.46));
    night=mix(vec3(0.012,0.026,0.052),night,smoothstep(-0.10,0.12,ray.y));

    vec2 sphereUV=vec2(atan(ray.z,ray.x)/6.2831853+0.5,asin(clamp(ray.y,-1.0,1.0))/3.14159265+0.5);
    vec2 starGrid=sphereUV*vec2(520.0,260.0);
    vec2 starCell=floor(starGrid),starLocal=fract(starGrid)-0.5;
    float starSeed=h2(starCell),starRadius=mix(0.025,0.080,smoothstep(0.994,1.0,starSeed));
    float stars=(1.0-smoothstep(starRadius,starRadius*1.8,length(starLocal)))*step(0.9945,starSeed);
    float twinkle=0.68+0.32*sin(uTime*(0.55+starSeed*1.8)+starSeed*91.0);
    stars*=twinkle*smoothstep(0.015,0.18,ray.y);

    float milkyBand=exp(-abs(dot(ray,normalize(vec3(0.22,0.84,0.49))))*11.0);
    float milkyDust=fb(sphereUV*vec2(9.0,17.0)+43.0);
    night+=vec3(0.055,0.075,0.13)*milkyBand*milkyDust*0.30;

    float moonDisk=smoothstep(0.99952,0.99984,sun);
    float moonHalo=pow(sun,110.0);
    night+=vec3(0.72,0.84,1.00)*stars;
    night+=vec3(0.72,0.84,1.00)*moonHalo*0.24;
    night+=vec3(1.22,1.28,1.30)*moonDisk*3.2;

    if(ray.y>0.035){
        vec2 ncp=ray.xz/max(ray.y,0.055)*0.20+vec2(uTime*0.00065,-uTime*0.00028);
        float nc1=fb(ncp*2.0),nc2=fb(ncp*4.7+9.3),ne=ridged(ncp*7.1+18.0);
        float nbody=nc1*0.72+nc2*0.20+ne*0.08;
        float ncloud=smoothstep(0.575,0.80,nbody)*smoothstep(0.02,0.16,ray.y);
        float moonEdge=pow(sun,9.0);
        vec3 nightCloud=mix(vec3(0.026,0.043,0.075),vec3(0.16,0.20,0.29),0.22+0.78*moonEdge);
        night=mix(night,nightCloud,ncloud*0.52);
    }
    night=mix(night,vec3(0.035,0.072,0.125),exp(-max(ray.y,0.0)*18.0)*0.30);
    c=mix(c,night,clamp(uNightBlend,0.0,1.0));
    FragColor=vec4(c,1.0);
}
