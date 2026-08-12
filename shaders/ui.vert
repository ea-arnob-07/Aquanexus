layout(location=0) in vec2 aPos; layout(location=1) in vec4 aColor; out vec4 vColor; uniform vec2 uScreen;
void main(){ vec2 ndc=vec2(aPos.x/uScreen.x*2.0-1.0,1.0-aPos.y/uScreen.y*2.0); gl_Position=vec4(ndc,0,1); vColor=aColor; }
