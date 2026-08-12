layout(location=0) in vec3 aPos;
layout(location=2) in vec2 aUV;
uniform mat4 uModel;
uniform mat4 uLightSpace;
uniform float uTime;
uniform int uMaterialMode;
void main(){
    vec4 wp=uModel*vec4(aPos,1.0);
    if(uMaterialMode==3 || uMaterialMode==9){
        float tip=clamp(aUV.y,0.0,1.0);
        float bend=tip*tip;
        vec3 rest=wp.xyz;
        float gust=0.54+0.46*sin(uTime*0.21+rest.x*0.043-rest.z*0.031);
        float broad=sin(rest.x*0.31+rest.z*0.27+uTime*1.18);
        float crossWind=sin(rest.z*0.67-rest.x*0.12-uTime*1.73);
        float flutter=sin((rest.x+rest.z)*2.6+uTime*4.7+tip*5.0);
        float sx=(broad+0.42*crossWind)*gust*0.050+flutter*0.008*tip;
        float sz=(crossWind*0.58+cos(rest.x*0.23+uTime*0.91))*gust*0.030;
        wp.x += sx*bend;
        wp.z += sz*bend;
        wp.y -= abs(broad)*0.006*bend;
    }
    gl_Position=uLightSpace*wp;
}
