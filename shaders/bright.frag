in vec2 vUV; out vec4 FragColor; uniform sampler2D uScene;
void main(){ vec3 c=texture(uScene,vUV).rgb; float b=max(c.r,max(c.g,c.b)); float w=smoothstep(1.05,2.5,b); FragColor=vec4(c*w,1); }
