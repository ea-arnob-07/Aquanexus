#pragma once
#include "Common.hpp"
#include "Camera.hpp"
#include "Scene.hpp"
#include "Simulation.hpp"
#include "UI.hpp"

namespace aqua {

class App {
public:
    App(int width=1600,int height=960);
    ~App();
    void run();
    void frame();

private:
    GLFWwindow* window_=nullptr;
    int width_=1600,height_=960;
    Simulation sim_;
    CameraRig camera_;
    std::unique_ptr<SceneRenderer> scene_;
    std::unique_ptr<UI> ui_;
    std::array<unsigned char,GLFW_KEY_LAST+1> prevKeys_{};
    bool prevMouseLeft_=false;
    bool showUI_=true;
    bool sensorHudExpanded_=false;
    bool nightMode_=false;
    double lastTime_=0.0;
    float fpsSmooth_=60.0f;
    float simSpeed_=1.0f;
    bool paused_=false;
    bool fullscreen_=false;
    int windowedX_=100,windowedY_=100,windowedW_=1600,windowedH_=960;

    void initWindow();
    void processInput(float dt);
    void drawUI();
    void toggleFullscreen();
    void setEnvironmentMode(bool night);
    bool keyPressed(int key) const;
    static void scrollCallback(GLFWwindow* window,double xoff,double yoff);
    static void framebufferCallback(GLFWwindow* window,int width,int height);
    void onScroll(double yoff);
    static std::string fnum(float v,int precision=2);
};

} // namespace aqua
