layout(location=0) in vec3 aPos;
layout(location=2) in vec2 aUV;
out vec2 vUV;
void main(){ vUV=aUV; gl_Position=vec4(aPos.x*2.0,aPos.z*2.0,1.0,1.0); }
