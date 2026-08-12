#pragma once
#include "Common.hpp"
#include "Mesh.hpp"
#include "Shader.hpp"
#include "Simulation.hpp"
#include "Camera.hpp"
#include "UI.hpp"

namespace aqua {

struct PipePath {
    std::array<glm::vec3,4> control;
    glm::vec3 point(float t) const { return bezierPoint(control,t); }
    glm::vec3 tangent(float t) const { return bezierTangent(control,t); }
};

class SceneRenderer {
public:
    SceneRenderer(int width,int height);
    ~SceneRenderer();
    SceneRenderer(const SceneRenderer&)=delete;
    SceneRenderer& operator=(const SceneRenderer&)=delete;

    void resize(int width,int height);
    void setNightMode(bool enabled);
    bool nightMode() const { return nightMode_; }
    void render(const Simulation& sim,const CameraRig& camera);
    void appendWorldLabels(UI& ui,const Simulation& sim,const CameraRig& camera,int width,int height) const;
    const std::array<PipePath,3>& pipePaths() const { return pipes_; }

private:
    int width_=1,height_=1;
    Mesh cube_,plane_,waterPlane_,sphere_,cylinder_,cone_,trapezoid_,leaf_,disc_,screenQuad_;
    Mesh pondBank_, rock_, grassPatch_, ricePatch_;
    std::array<Mesh,3> pipeOuter_;
    std::array<Mesh,3> pipeInner_;
    Shader pbr_,shadow_,sky_,water_,glass_,bright_,blur_,post_;

    GLuint shadowFBO_=0,shadowTex_=0;
    GLuint opaqueFBO_=0,opaqueColor_=0,opaqueDepth_=0;
    GLuint sceneFBO_=0,sceneColor_=0,sceneDepthRBO_=0;
    GLuint bloomFBO_[2]{0,0}, bloomTex_[2]{0,0};
    int fboW_=0,fboH_=0;
    const int shadowSize_=4096;

    glm::vec3 sunDir_{-0.40f,-0.87f,-0.28f};
    glm::vec3 sunColor_{5.1f,4.55f,3.70f};
    bool nightMode_=false;
    std::array<PipePath,3> pipes_;
    std::vector<glm::vec3> trees_;
    std::vector<glm::vec3> bushes_;
    std::vector<glm::vec3> huts_;
    std::vector<glm::vec3> fencePosts_;

    struct RenderGlobals { glm::mat4 view,proj,lightSpace; glm::vec3 cameraPos; };
    struct Material { glm::vec3 color; float roughness; float metallic; int mode; bool receiveShadow=true; };

    void initWorld();
    void initShadowFBO();
    void recreateSceneFBOs();
    void destroyFBOs();
    static Mesh makeTube(const PipePath& path,float radius,int rings,int sides);

    glm::mat4 lightSpaceMatrix() const;
    void beginPBR(const RenderGlobals& g,float time);
    void drawObject(const Mesh& mesh,const glm::mat4& model,const Material& mat,bool shadowPass);
    void drawOpaqueGeometry(const Simulation& sim,const RenderGlobals& g,bool shadowPass);
    void drawPondStructure(const PondState& p,int index,bool shadowPass);
    void drawVillage(bool shadowPass);
    void drawPalms(bool shadowPass);
    void drawHuts(bool shadowPass);
    void drawCrops(bool shadowPass);
    void drawDenseVegetation(bool shadowPass);
    void drawVillageLife(bool shadowPass);
    void drawFish(const Simulation& sim,bool shadowPass);
    void drawDebris(const Simulation& sim,bool shadowPass);
    void drawSensors(const Simulation& sim,bool shadowPass);
    void drawMonitoringStation(const Simulation& sim,bool shadowPass);
    void drawFans(const Simulation& sim,bool shadowPass);

    void drawSky(const RenderGlobals& g,float time);
    void drawWater(const Simulation& sim,const RenderGlobals& g);
    void drawTransparentPipes(const Simulation& sim,const RenderGlobals& g);
    void drawBloom();
    void drawPost(float time);

    bool pointInsidePondFootprint(const glm::vec3& p,float margin=0.0f) const;
    bool rectIntersectsPond(const glm::vec3& p,float halfX,float halfZ,float margin=0.0f) const;
    glm::vec2 pondWaterHalfSize(int index) const;

    glm::mat4 modelBox(const glm::vec3& p,const glm::vec3& scale) const;
    glm::mat4 modelCylinderZ(const glm::vec3& p,const glm::vec3& dir,float radius,float length) const;
    glm::mat4 modelCylinderY(const glm::vec3& p,float radius,float height) const;
};

} // namespace aqua
