#pragma once
#include "Common.hpp"

namespace aqua {

class CameraRig {
public:
    // Default oblique aerial composition is tuned to match the supplied reference image.
    glm::vec3 target{0.0f,1.25f,0.3f};
    float yaw=-0.10f;
    float pitch=0.66f;
    float distance=72.0f;
    float fov=44.0f;
    bool cinematic=false;

    glm::vec3 position() const;
    glm::mat4 view() const;
    glm::mat4 projection(float aspect) const;
    void update(GLFWwindow* window,float dt);
    void reset();
    void setCinematic(bool enabled);

private:
    bool dragging_=false;
    double lastX_=0,lastY_=0;
    float cinematicTime_=0.0f;

    void applyEyeTarget(const glm::vec3& eye,const glm::vec3& target,float nextFov);
    void updateAutoTour(float dt);
};

} // namespace aqua
