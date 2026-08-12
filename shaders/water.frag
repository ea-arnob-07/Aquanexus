in vec3 vWorldPos;
in vec3 vNormal;
in vec2 vUV;
out vec4 FragColor;
uniform vec3 uCameraPos;
uniform vec3 uSunDir;
uniform vec3 uSunColor;
uniform sampler2D uSceneColor;
uniform sampler2D uSceneDepth;
uniform vec2 uScreenSize;
uniform float uTime;
uniform float uFlowStrength;
uniform float uTurbidity;
uniform float uQuality;
uniform float uNear;
uniform float uFar;
float h2(vec2 p){ return fract(sin(dot(p,vec2(127.1,311.7)))*43758.5453); }
float n2(vec2 p){ vec2 i=floor(p),f=fract(p);f=f*f*(3.0-2.0*f);return mix(mix(h2(i),h2(i+vec2(1,0)),f.x),mix(h2(i+vec2(0,1)),h2(i+vec2(1,1)),f.x),f.y); }
float linearDepth(float d){ float z=d*2.0-1.0; return (2.0*uNear*uFar)/(uFar+uNear-z*(uFar-uNear)); }
vec3 skyReflection(vec3 d){
    float y=clamp(d.y*0.5+0.5,0.0,1.0); vec3 c=mix(vec3(0.62,0.76,0.88),vec3(0.06,0.28,0.60),pow(y,0.62));
    float sun=max(dot(normalize(d),normalize(-uSunDir)),0.0); c+=vec3(1.0,0.84,0.61)*pow(sun,800.0)*6.5; return c;
}
void main(){
    vec3 N=normalize(vNormal);
    float n1=n2(vWorldPos.xz*10.0+vec2(uTime*0.21,-uTime*0.16));
    float n2v=n2(vWorldPos.xz*18.0+vec2(-uTime*0.13,uTime*0.20));
    float n3=n2(vWorldPos.xz*31.0+vec2(uTime*0.07,uTime*0.11));
    N=normalize(N+vec3((n1-0.5)*0.085+(n3-0.5)*0.026,0.0,(n2v-0.5)*0.085-(n3-0.5)*0.022));
    vec3 V=normalize(uCameraPos-vWorldPos), L=normalize(-uSunDir), H=normalize(V+L);
    float NoV=max(dot(N,V),0.0); float fres=0.0204+(1.0-0.0204)*pow(1.0-NoV,5.0);
    vec2 suv=gl_FragCoord.xy/uScreenSize;
    vec2 distort=N.xz*(0.010+0.008*uFlowStrength)*(0.35+0.65*(1.0-NoV));
    vec3 refr=texture(uSceneColor,clamp(suv+distort,vec2(0.002),vec2(0.998))).rgb;
    float sd=texture(uSceneDepth,suv).r; float wd=gl_FragCoord.z;
    float thick=max(linearDepth(sd)-linearDepth(wd),0.0);
    thick=clamp(thick,0.0,5.0);

    // Deeper blue-green water: stronger absorption and a more pronounced deep-pond falloff.
    vec3 absorb=mix(vec3(0.20,0.10,0.040),vec3(0.31,0.13,0.060),clamp(uTurbidity,0.0,1.0)*0.55);
    vec3 trans=exp(-absorb*thick);
    vec3 scatter=mix(vec3(0.08,0.34,0.74),vec3(0.16,0.54,0.90),clamp(uQuality,0.0,1.0));
    scatter=mix(scatter,vec3(0.06,0.16,0.20),clamp(uTurbidity,0.0,1.0)*0.18);
    vec3 refracted=refr*trans + scatter*(1.0-trans);
    vec3 reflected=skyReflection(reflect(-V,N));

    float NoH=max(dot(N,H),0.0);
    float sunSpec=pow(NoH,380.0)*7.5 + pow(NoH,1500.0)*16.0*(0.55+0.45*n1);
    float shore=1.0-smoothstep(0.08,0.55,thick);
    float foamNoise=n2(vWorldPos.xz*5.5+uTime*0.08);
    float foam=shore*smoothstep(0.38,0.76,foamNoise+0.22)*0.70;
    vec3 body=mix(refracted,reflected,clamp(fres*1.04,0.0,0.92));
    vec3 deepColor=mix(vec3(0.02,0.12,0.42),vec3(0.02,0.07,0.21),clamp(uTurbidity,0.0,1.0)*0.42);
    body=mix(body,deepColor,smoothstep(1.2,4.6,thick)*0.72);
    body+=uSunColor*sunSpec;
    body=mix(body,vec3(0.88,0.95,0.96),foam);
    float alpha=mix(0.86,0.985,clamp(thick*0.55,0.0,1.0));
    FragColor=vec4(body,alpha);
}
