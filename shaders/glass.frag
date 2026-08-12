in vec3 vWorldPos;
in vec3 vNormal;
out vec4 FragColor;
uniform vec3 uCameraPos;
uniform vec3 uTint;
uniform float uAlpha;
uniform float uPulse;
uniform vec3 uSunDir;
uniform sampler2D uSceneColor;
uniform vec2 uScreenSize;
float hash21(vec2 p){
    p=fract(p*vec2(123.34,456.21)); p+=dot(p,p+45.32); return fract(p.x*p.y);
}
void main(){
    vec3 N=normalize(vNormal),V=normalize(uCameraPos-vWorldPos); float facing=abs(dot(N,V));
    float fres=0.028+0.972*pow(1.0-facing,5.0); vec2 suv=gl_FragCoord.xy/uScreenSize;
    float wav=sin(vWorldPos.z*31.0+vWorldPos.x*17.0+vWorldPos.y*23.0+uPulse*4.0)*0.5+0.5;
    vec2 off=N.xy*(0.0032+0.0052*(1.0-facing))*(0.88+0.12*wav);
    // Subtle chromatic separation and a second internal bounce make the acrylic pipe feel thick.
    float ca=0.00065*(1.0-facing);
    float rr=texture(uSceneColor,clamp(suv+off+vec2(ca,0),vec2(.002),vec2(.998))).r;
    float gg=texture(uSceneColor,clamp(suv+off,vec2(.002),vec2(.998))).g;
    float bb=texture(uSceneColor,clamp(suv+off-vec2(ca,0),vec2(.002),vec2(.998))).b;
    vec3 refr=vec3(rr,gg,bb);
    vec3 internal=texture(uSceneColor,clamp(suv-off*0.42,vec2(.002),vec2(.998))).rgb;
    refr=mix(refr,internal,0.13+0.09*(1.0-facing));
    vec3 sky=mix(vec3(.73,.85,.91),vec3(.11,.38,.66),clamp(reflect(-V,N).y*.5+.5,0.0,1.0));
    vec3 L=normalize(-uSunDir),H=normalize(V+L);
    float spec=pow(max(dot(N,H),0.0),260.0)*3.9+pow(max(dot(N,H),0.0),52.0)*0.20;
    float scratch=pow(hash21(floor(vWorldPos.xy*48.0)+floor(vWorldPos.z*21.0)),18.0);
    float rimBand=smoothstep(0.52,0.96,1.0-facing);
    vec3 c=mix(refr*uTint,sky,fres*0.66)+vec3(1.0,.98,.94)*spec;
    c+=uTint*(0.10+0.11*uPulse)+vec3(0.76,0.88,0.92)*rimBand*0.075;
    c=mix(c,c*0.74,scratch*0.10);
    float a=uAlpha*(0.18+0.82*fres)+scratch*0.015;
    FragColor=vec4(c,a);
}
