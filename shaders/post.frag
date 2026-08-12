in vec2 vUV; out vec4 FragColor;
uniform sampler2D uScene; uniform sampler2D uBloom; uniform sampler2D uDepth;
uniform vec2 uScreenSize; uniform float uTime; uniform float uExposure; uniform float uNear; uniform float uFar;
vec3 aces(vec3 x){ float a=2.51,b=.03,c=2.43,d=.59,e=.14; return clamp((x*(a*x+b))/(x*(c*x+d)+e),0.0,1.0); }
float lum(vec3 c){return dot(c,vec3(.2126,.7152,.0722));}
float linearDepth(float d){float z=d*2.0-1.0;return(2.0*uNear*uFar)/(uFar+uNear-z*(uFar-uNear));}
void main(){
    vec2 px=1.0/uScreenSize; vec3 c=texture(uScene,vUV).rgb; vec3 b=texture(uBloom,vUV).rgb; c+=b*0.105;
    vec3 n=texture(uScene,vUV+vec2(0,px.y)).rgb, s=texture(uScene,vUV-vec2(0,px.y)).rgb;
    vec3 e=texture(uScene,vUV+vec2(px.x,0)).rgb, w=texture(uScene,vUV-vec2(px.x,0)).rgb;
    vec3 avg=(n+s+e+w)*0.25;
    float lc=lum(c), lmin=min(lc,min(min(lum(n),lum(s)),min(lum(e),lum(w))));
    float lmax=max(lc,max(max(lum(n),lum(s)),max(lum(e),lum(w))));
    float edge=clamp((lmax-lmin)*2.6,0.0,0.20);
    c=mix(c,avg,edge);       // FXAA-style local edge softening
    c += (c-avg)*0.065;      // recover fine photographic detail after AA

    // Lightweight depth-contact AO: gives pond banks, rocks, equipment and vegetation grounding.
    float d0=texture(uDepth,vUV).r; float ao=1.0;
    if(d0<0.9999){
        float z0=linearDepth(d0), occ=0.0;
        vec2 offs[8]=vec2[8](vec2(1,0),vec2(-1,0),vec2(0,1),vec2(0,-1),vec2(1,1),vec2(-1,1),vec2(1,-1),vec2(-1,-1));
        for(int i=0;i<8;i++){
            float zi=linearDepth(texture(uDepth,vUV+offs[i]*px*3.0).r);
            float dz=z0-zi; occ+=smoothstep(0.02,0.55,dz)*exp(-abs(dz)*0.75);
        }
        ao=1.0-clamp(occ/8.0,0.0,1.0)*0.22;
    }
    c*=ao;
    c=vec3(1.0)-exp(-c*max(uExposure,0.1)); c=aces(c*1.03);
    float l=lum(c); c=mix(vec3(l),c,0.96); // restrained saturation, photographic rather than cartoon
    c=pow(max(c,vec3(0)),vec3(1.0/2.2)); c=(c-0.5)*1.035+0.5;
    c*=vec3(1.015,1.005,0.985);
    float vig=smoothstep(0.42,0.82,distance(vUV,vec2(.5))); c*=1.0-vig*.13;
    float grain=fract(sin(dot(gl_FragCoord.xy+uTime,vec2(12.9898,78.233)))*43758.5453)-.5; c+=grain*.0035;
    FragColor=vec4(clamp(c,0.0,1.0),1);
}
