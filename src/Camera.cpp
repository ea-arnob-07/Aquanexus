#include "Camera.hpp"

namespace aqua {
namespace {
struct TourKeyframe {
    glm::vec3 eye;
    glm::vec3 target;
    float fov;
    float duration;
};

float smooth01(float t){ return t*t*(3.0f-2.0f*t); }

const TourKeyframe kTour[] = {
    // Pond 1: very close, slightly elevated, strong detail on surface and bank.
    {{-34.6f, 4.8f, 11.8f}, {-24.2f, 0.45f, 4.7f}, 33.0f, 4.8f},
    // Travel in low cinematic arc toward the center pond.
    {{-18.0f, 5.2f,  7.6f}, {-10.5f, 0.70f, 2.4f}, 34.0f, 3.6f},
    // Pond 2: close inspection of sensor pond.
    {{ -8.1f, 4.4f,  5.4f}, {  0.0f, 0.55f, 0.7f}, 32.0f, 4.8f},
    // Travel low along the chain to Pond 3.
    {{ 10.4f, 4.8f,  2.4f}, { 16.8f, 0.70f,-1.6f}, 34.0f, 3.6f},
    // Pond 3: close, detailed final stop.
    {{ 16.7f, 4.6f,  2.6f}, { 24.9f, 0.48f,-3.8f}, 32.0f, 5.0f},
    // Return path to restart the loop cleanly.
    {{-26.0f, 6.0f, 12.5f}, { -9.0f, 1.15f, 0.4f}, 36.0f, 4.4f}
};
constexpr int kTourCount = sizeof(kTour)/sizeof(kTour[0]);
} // namespace

glm::vec3 CameraRig::position() const {
    float cp=std::cos(pitch), sp=std::sin(pitch);
    glm::vec3 dir(cp*std::sin(yaw), sp, cp*std::cos(yaw));
    return target+dir*distance;
}

glm::mat4 CameraRig::view() const { return glm::lookAt(position(),target,glm::vec3(0,1,0)); }
glm::mat4 CameraRig::projection(float aspect) const { return glm::perspective(glm::radians(fov),aspect,0.08f,240.0f); }

void CameraRig::applyEyeTarget(const glm::vec3& eye,const glm::vec3& newTarget,float nextFov){
    glm::vec3 off=eye-newTarget;
    distance=glm::length(off);
    if(distance<0.001f) distance=0.001f;
    float horiz=std::sqrt(off.x*off.x+off.z*off.z);
    yaw=std::atan2(off.x,off.z);
    pitch=std::atan2(off.y,horiz);
    pitch=clampf(pitch,0.08f,1.50f);
    target=newTarget;
    fov=nextFov;
}

void CameraRig::updateAutoTour(float dt){
    cinematicTime_ += dt;
    float total=0.0f;
    for(int i=0;i<kTourCount;++i) total += kTour[i].duration;
    if(total<=0.001f) return;
    float t=std::fmod(cinematicTime_,total);
    int seg=0;
    while(seg<kTourCount-1 && t>kTour[seg].duration){
        t-=kTour[seg].duration;
        ++seg;
    }
    int next=(seg+1)%kTourCount;
    float u=smooth01(clampf(t/kTour[seg].duration,0.0f,1.0f));
    glm::vec3 eye=glm::mix(kTour[seg].eye,kTour[next].eye,u);
    glm::vec3 tgt=glm::mix(kTour[seg].target,kTour[next].target,u);
    float nextFov=lerpf(kTour[seg].fov,kTour[next].fov,u);
    // subtle floating movement so the path feels more cinematic than robotic
    eye.y += std::sin(cinematicTime_*0.55f + seg*0.8f)*0.08f;
    tgt.y += std::sin(cinematicTime_*0.41f + seg*0.6f)*0.04f;
    applyEyeTarget(eye,tgt,nextFov);
}

void CameraRig::setCinematic(bool enabled){
    cinematic=enabled;
    dragging_=false;
    if(cinematic) cinematicTime_=0.0f;
}

void CameraRig::reset(){
    target={0.0f,1.25f,0.3f};
    yaw=-0.10f;
    pitch=0.66f;
    distance=72.0f;
    fov=44.0f;
    cinematic=false;
    cinematicTime_=0.0f;
    dragging_=false;
}

void CameraRig::update(GLFWwindow* w,float dt){
    if(cinematic){
        updateAutoTour(dt);
        return;
    }

    double x,y; glfwGetCursorPos(w,&x,&y);
    bool down=(glfwGetMouseButton(w,GLFW_MOUSE_BUTTON_RIGHT)==GLFW_PRESS) ||
              (glfwGetMouseButton(w,GLFW_MOUSE_BUTTON_LEFT)==GLFW_PRESS);
    if(down&&!dragging_){ dragging_=true; lastX_=x; lastY_=y; }
    if(!down) dragging_=false;
    if(dragging_){
        double dx=x-lastX_,dy=y-lastY_; lastX_=x; lastY_=y;
        yaw-=float(dx)*0.0047f; pitch+=float(dy)*0.0042f;
        // Wider pitch range gives a much freer screen rotation feel.
        pitch=clampf(pitch,0.08f,1.50f);
    }
    glm::vec3 pos=position();
    glm::vec3 forward=glm::normalize(glm::vec3(target.x-pos.x,0,target.z-pos.z));
    glm::vec3 right=glm::normalize(glm::cross(forward,glm::vec3(0,1,0)));
    float move=10.0f*dt*(glfwGetKey(w,GLFW_KEY_LEFT_SHIFT)==GLFW_PRESS?2.2f:1.0f);
    // Arrow keys own camera panning so D remains an unambiguous DAY command.
    if(glfwGetKey(w,GLFW_KEY_UP)==GLFW_PRESS) target+=forward*move;
    if(glfwGetKey(w,GLFW_KEY_DOWN)==GLFW_PRESS) target-=forward*move;
    if(glfwGetKey(w,GLFW_KEY_RIGHT)==GLFW_PRESS) target+=right*move;
    if(glfwGetKey(w,GLFW_KEY_LEFT)==GLFW_PRESS) target-=right*move;
    if(glfwGetKey(w,GLFW_KEY_Q)==GLFW_PRESS) target.y+=move*0.6f;
    if(glfwGetKey(w,GLFW_KEY_E)==GLFW_PRESS) target.y-=move*0.6f;
    target.x=clampf(target.x,-48.0f,48.0f); target.z=clampf(target.z,-36.0f,36.0f); target.y=clampf(target.y,0.0f,16.0f);
}

} // namespace aqua
